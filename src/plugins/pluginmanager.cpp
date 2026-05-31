#include "pluginmanager.h"
#include "pluginformat.h"
#include "../core/logger.h"
#include "../core/types.h"
#include "../ui/thememanager.h"
#include <QDir>
#include <QFileInfo>
#include <QPluginLoader>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QApplication>

// ── PluginHost ────────────────────────────────────────────────────────────

void PluginHost::log(const QString& msg) {
    Logger::instance().info(LogComponent::General, msg);
}

QString PluginHost::appVersion() const {
    return QStringLiteral(APP_VERSION);
}

QString PluginHost::pluginsDir() const {
    return PluginManager::pluginsDir();
}

// ── PluginManager ─────────────────────────────────────────────────────────

PluginManager& PluginManager::instance() {
    static PluginManager inst;
    return inst;
}

PluginManager::PluginManager(QObject* parent) : QObject(parent) {}

QString PluginManager::pluginsDir() {
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
           + QStringLiteral("/plugins");
}

QString PluginManager::extractedDir(const QString& pluginId) {
    // Проверяем id перед использованием как компонент пути
    // Допустимо: буквы, цифры, точки, дефисы, подчёркивания — без слэшей и ..
    static const QRegularExpression kValidId(QStringLiteral("^[A-Za-z0-9._-]+$"));
    if (!kValidId.match(pluginId).hasMatch()) return {};

    const QString base  = pluginsDir() + QStringLiteral("/.extracted/");
    const QString cand  = QDir::cleanPath(base + pluginId);
    // Канонически проверяем что путь не вышел за base
    if (!cand.startsWith(QDir::cleanPath(base))) return {};

    return cand;
}

void PluginManager::loadAll() {
    loadEnabledState();

    const QString dir = pluginsDir();
    QDir d(dir);
    if (!d.exists()) {
        QDir().mkpath(dir);
        m_loaded = true;
        emit pluginsChanged();
        return;
    }

    for (const QFileInfo& fi : d.entryInfoList({QStringLiteral("*.plugin")}, QDir::Files)) {
        const PluginMeta meta = PluginFormat::readMeta(fi.absoluteFilePath());
        if (!meta.isValid()) {
            Logger::instance().info(LogComponent::General,
                QStringLiteral("[PluginManager] Невалидный .plugin: %1 — %2")
                    .arg(fi.fileName(), PluginFormat::lastError()));
            continue;
        }

        // Проверяем id безопасен для использования как имя директории
        if (extractedDir(meta.id).isEmpty()) {
            Logger::instance().info(LogComponent::General,
                QStringLiteral("[PluginManager] Недопустимый id плагина (path traversal?): %1")
                    .arg(meta.id));
            continue;
        }

        PluginEntry entry;
        entry.meta     = meta;
        entry.filePath = fi.absoluteFilePath();
        entry.enabled  = m_enabledState.value(meta.id, true);

        if (meta.encrypted) {
            // Есть ли сохранённый на сессию ключ?
            if (m_keys.contains(meta.id)) {
                if (extractAndLoad(entry, m_keys[meta.id])) {
                    entry.state = entry.enabled ? PluginState::Active : PluginState::Inactive;
                } else {
                    entry.state = PluginState::Error;
                }
            } else {
                entry.state = PluginState::Locked;
            }
        } else {
            if (extractAndLoad(entry, {})) {
                entry.state = entry.enabled ? PluginState::Active : PluginState::Inactive;
            } else {
                entry.state = PluginState::Error;
            }
        }

        m_plugins.append(entry);
        Logger::instance().info(LogComponent::General,
            QStringLiteral("[PluginManager] %1: %2 v%3")
                .arg(stateToStr(entry.state), meta.name, meta.version));
    }

    m_loaded = true;
    emit pluginsChanged();

    if (!collectExtraCSS().isEmpty())
        qApp->setStyleSheet(ThemeManager::instance().stylesheet());
}

void PluginManager::unloadAll() {
    for (auto& e : m_plugins) {
        if (e.plugin && e.state == PluginState::Active)
            e.plugin->shutdown();
        if (e.loader) {
            e.loader->unload();
            e.loader->deleteLater();
        }
    }
    m_plugins.clear();
    m_loaded = false;
    emit pluginsChanged();
}

void PluginManager::reload() {
    unloadAll();
    loadAll();
    emit reloaded();
}

bool PluginManager::unlock(const QString& id, const QString& key) {
    if (!PluginFormat::verifyKey(pluginFilePath(id), key)) {
        Logger::instance().info(LogComponent::General,
            QStringLiteral("[PluginManager] Неверный ключ для плагина: ") + id);
        return false;
    }

    m_keys[id] = key;

    for (auto& e : m_plugins) {
        if (e.meta.id != id) continue;
        if (extractAndLoad(e, key)) {
            e.state = e.enabled ? PluginState::Active : PluginState::Inactive;
            emit pluginsChanged();
            if (!collectExtraCSS().isEmpty())
                qApp->setStyleSheet(ThemeManager::instance().stylesheet());
            return true;
        }
        e.state = PluginState::Error;
        emit pluginsChanged();
        return false;
    }
    return false;
}

void PluginManager::setEnabled(const QString& id, bool enabled) {
    m_enabledState[id] = enabled;
    saveEnabledState();

    for (auto& e : m_plugins) {
        if (e.meta.id != id) continue;
        if (e.state == PluginState::Locked || e.state == PluginState::Error) break;

        if (enabled && e.state == PluginState::Inactive) {
            if (e.plugin) {
                e.plugin->initialize(&m_host);
                e.state = PluginState::Active;
            }
        } else if (!enabled && e.state == PluginState::Active) {
            if (e.plugin) {
                e.plugin->shutdown();
                e.state = PluginState::Inactive;
            }
        }
        e.enabled = enabled;
        break;
    }

    emit pluginsChanged();
    qApp->setStyleSheet(ThemeManager::instance().stylesheet());
}

bool PluginManager::isEnabled(const QString& id) const {
    return m_enabledState.value(id, true);
}

QString PluginManager::collectExtraCSS() const {
    QString css;
    for (const auto& e : m_plugins) {
        if (e.state != PluginState::Active || !e.plugin) continue;
        const QString extra = e.plugin->extraStyleSheet();
        if (!extra.isEmpty()) {
            css += QStringLiteral("\n/* Plugin: ") + e.meta.id + QStringLiteral(" */\n");
            css += extra;
        }
    }
    return css;
}

void PluginManager::loadEnabledState() {
    const QString path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                       + QStringLiteral("/plugins.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    const auto obj = QJsonDocument::fromJson(f.readAll()).object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        m_enabledState[it.key()] = it.value().toBool(true);
}

void PluginManager::saveEnabledState() const {
    const QString path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                       + QStringLiteral("/plugins.json");
    QJsonObject obj;
    for (auto it = m_enabledState.constBegin(); it != m_enabledState.constEnd(); ++it)
        obj[it.key()] = it.value();
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(obj).toJson());
}

// ── Приватные хелперы ─────────────────────────────────────────────────────

bool PluginManager::extractAndLoad(PluginEntry& entry, const QString& key) {
    const QString destDir = extractedDir(entry.meta.id);

    // Проверяем нужно ли переизвлечь
    const QString soPath = nativeSoPath(destDir);
    QFileInfo soInfo(soPath);
    QFileInfo pluginInfo(entry.filePath);
    const bool needExtract = !soInfo.exists() ||
                             soInfo.lastModified() < pluginInfo.lastModified();

    if (needExtract) {
        if (!PluginFormat::extract(entry.filePath, destDir, key)) {
            Logger::instance().info(LogComponent::General,
                QStringLiteral("[PluginManager] Ошибка распаковки %1: %2")
                    .arg(entry.meta.id, PluginFormat::lastError()));
            return false;
        }
    }

    if (!QFileInfo::exists(soPath)) {
        Logger::instance().info(LogComponent::General,
            QStringLiteral("[PluginManager] .so не найден после распаковки: ") + soPath);
        return false;
    }

    auto* loader = new QPluginLoader(soPath, this);
    QObject* obj = loader->instance();
    auto* plugin = obj ? qobject_cast<IPlugin*>(obj) : nullptr;

    if (!plugin) {
        Logger::instance().info(LogComponent::General,
            QStringLiteral("[PluginManager] Не IPlugin (%1): %2")
                .arg(entry.meta.id, loader->errorString()));
        loader->unload();
        delete loader;
        return false;
    }

    if (entry.enabled)
        plugin->initialize(&m_host);

    entry.plugin = plugin;
    entry.loader = loader;
    return true;
}

QString PluginManager::nativeSoPath(const QString& dir) {
#if defined(Q_OS_WIN)
    return dir + QStringLiteral("/plugin.dll");
#else
    return dir + QStringLiteral("/plugin.so");
#endif
}

QString PluginManager::pluginFilePath(const QString& id) const {
    for (const auto& e : m_plugins) {
        if (e.meta.id == id) return e.filePath;
    }
    return {};
}

QString PluginManager::stateToStr(PluginState s) {
    switch (s) {
        case PluginState::Active:   return QStringLiteral("Active");
        case PluginState::Inactive: return QStringLiteral("Inactive");
        case PluginState::Locked:   return QStringLiteral("Locked");
        case PluginState::Error:    return QStringLiteral("Error");
    }
    return {};
}
