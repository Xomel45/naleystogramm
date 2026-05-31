#include "pluginmanager.h"
#include "../core/logger.h"
#include "../core/types.h"
#include "../ui/thememanager.h"
#include <QPluginLoader>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QApplication>

// ── PluginHost ────────────────────────────────────────────────────────────

void PluginHost::log(const QString& msg) {
    Logger::instance().info(LogComponent::General,"[Plugin] " + msg);
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

#if defined(Q_OS_WIN)
    const QStringList filters = {QStringLiteral("*.dll")};
#else
    const QStringList filters = {QStringLiteral("*.so")};
#endif

    for (const QFileInfo& fi : d.entryInfoList(filters, QDir::Files)) {
        auto* loader = new QPluginLoader(fi.absoluteFilePath(), this);
        QObject* obj = loader->instance();
        if (!obj) {
            Logger::instance().info(LogComponent::General,QStringLiteral("[PluginManager] Ошибка загрузки: %1 — %2")
                .arg(fi.fileName(), loader->errorString()));
            delete loader;
            continue;
        }
        auto* plugin = qobject_cast<IPlugin*>(obj);
        if (!plugin) {
            Logger::instance().info(LogComponent::General,QStringLiteral("[PluginManager] Не IPlugin: %1")
                .arg(fi.fileName()));
            loader->unload();
            delete loader;
            continue;
        }

        const bool enabled = m_enabledState.value(plugin->id(), true);
        if (enabled)
            plugin->initialize(&m_host);

        m_plugins.append({plugin, loader, fi.absoluteFilePath(), enabled});
        Logger::instance().info(LogComponent::General,QStringLiteral("[PluginManager] Загружен: %1 v%2 (%3)")
            .arg(plugin->name(), plugin->version(), plugin->id()));
    }

    m_loaded = true;
    emit pluginsChanged();

    // Перекрашиваем тему если хоть один плагин добавляет CSS
    if (!collectExtraCSS().isEmpty())
        qApp->setStyleSheet(ThemeManager::instance().stylesheet());
}

void PluginManager::unloadAll() {
    for (auto& e : m_plugins) {
        if (e.enabled && e.plugin)
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

void PluginManager::setEnabled(const QString& id, bool enabled) {
    m_enabledState[id] = enabled;
    saveEnabledState();

    for (auto& e : m_plugins) {
        if (e.plugin && e.plugin->id() == id) {
            if (enabled && !e.enabled)
                e.plugin->initialize(&m_host);
            else if (!enabled && e.enabled)
                e.plugin->shutdown();
            e.enabled = enabled;
            break;
        }
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
        if (!e.enabled || !e.plugin) continue;
        const QString extra = e.plugin->extraStyleSheet();
        if (!extra.isEmpty()) {
            css += QStringLiteral("\n/* Plugin: ") + e.plugin->id() + QStringLiteral(" */\n");
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
