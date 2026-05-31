#pragma once
#include "iplugin.h"
#include <QObject>
#include <QList>
#include <QMap>
#include <QString>

class QPluginLoader;

// ── PluginEntry ───────────────────────────────────────────────────────────

struct PluginEntry {
    IPlugin*       plugin   {nullptr};
    QPluginLoader* loader   {nullptr};
    QString        filePath;
    bool           enabled  {true};
};

// ── PluginHost ────────────────────────────────────────────────────────────

class PluginHost final : public IPluginHost {
public:
    void    log(const QString& msg)   override;
    QString appVersion()        const override;
    QString pluginsDir()        const override;
};

// ── PluginManager ─────────────────────────────────────────────────────────
// Загружает .so/.dll из pluginsDir(), управляет жизненным циклом плагинов.
//
// Порядок инициализации в main.cpp:
//   PluginManager::instance().loadAll();   // после ensureDirectories()
//   PluginManager::instance().unloadAll(); // в деструкторе App (автоматически)

class PluginManager : public QObject {
    Q_OBJECT
public:
    static PluginManager& instance();

    // Загружает все .so/.dll из pluginsDir(). Вызывается один раз при старте.
    void loadAll();

    // Выгружает все плагины (shutdown + unload). Безопасно вызвать несколько раз.
    void unloadAll();

    // Повторная загрузка без перезапуска приложения.
    void reload();

    [[nodiscard]] QList<PluginEntry> plugins()  const { return m_plugins; }
    [[nodiscard]] bool               hasAny()   const { return !m_plugins.isEmpty(); }
    [[nodiscard]] bool               isLoaded() const { return m_loaded; }

    // Папка с плагинами: ~/.cache/naleystogramm/plugins/
    [[nodiscard]] static QString pluginsDir();

    // Включить / выключить плагин (persist в plugins.json)
    void setEnabled(const QString& id, bool enabled);
    [[nodiscard]] bool isEnabled(const QString& id) const;

    // Суммарный extra CSS от всех включённых плагинов.
    // Вызывается из ThemeManager::stylesheet() — безопасно до loadAll().
    [[nodiscard]] QString collectExtraCSS() const;

signals:
    void pluginsChanged();
    void reloaded();

private:
    explicit PluginManager(QObject* parent = nullptr);

    void loadEnabledState();
    void saveEnabledState() const;

    QList<PluginEntry>  m_plugins;
    QMap<QString, bool> m_enabledState;  // id → enabled
    PluginHost          m_host;
    bool                m_loaded {false};
};
