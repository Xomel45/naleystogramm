#pragma once
#include "iplugin.h"
#include "pluginformat.h"
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

class QPluginLoader;

// ── PluginState ───────────────────────────────────────────────────────────

enum class PluginState {
    Active,   // загружен, включён, initialize() вызван
    Inactive, // загружен, но выключен пользователем
    Locked,   // зашифрован, ключ не предоставлен
    Error,    // ошибка загрузки/распаковки
};

// ── PluginEntry ───────────────────────────────────────────────────────────

struct PluginEntry {
    PluginMeta     meta;
    QString        filePath;          // путь к .plugin файлу
    PluginState    state   {PluginState::Inactive};
    bool           enabled {true};    // пользовательский переключатель
    IPlugin*       plugin  {nullptr}; // null если Locked или Error
    QPluginLoader* loader  {nullptr};
};

// ── PluginHost ────────────────────────────────────────────────────────────

class PluginHost final : public IPluginHost {
public:
    void    log(const QString& msg)   override;
    QString appVersion()        const override;
    QString pluginsDir()        const override;
};

// ── PluginManager ─────────────────────────────────────────────────────────
// Сканирует pluginsDir() на .plugin файлы, управляет жизненным циклом.
//
// Особенности:
//  - readMeta() работает без ключа (метаданные всегда plaintext)
//  - Зашифрованные плагины отображаются как Locked до ввода ключа
//  - Ключи хранятся в памяти на время сессии (не сохраняются на диск)
//  - Извлечение .so → ~/.cache/naleystogramm/plugins/.extracted/<id>/

class PluginManager : public QObject {
    Q_OBJECT
public:
    static PluginManager& instance();

    // Сканирует папку, читает метаданные, загружает незашифрованные.
    // Вызывается один раз при старте.
    void loadAll();

    // Выгружает все плагины. Безопасно вызывать несколько раз.
    void unloadAll();

    // Полная перезагрузка (unloadAll + loadAll). Сохранённые ключи переиспользуются.
    void reload();

    // Попытка разблокировать зашифрованный плагин с ключом.
    // Возвращает true если ключ верный и плагин загружен.
    bool unlock(const QString& id, const QString& key);

    [[nodiscard]] QList<PluginEntry> plugins()  const { return m_plugins; }
    [[nodiscard]] bool               hasAny()   const { return !m_plugins.isEmpty(); }
    [[nodiscard]] bool               isLoaded() const { return m_loaded; }

    // Включить / выключить плагин (persist в plugins.json)
    void setEnabled(const QString& id, bool enabled);
    [[nodiscard]] bool isEnabled(const QString& id) const;

    // Папка с .plugin файлами: ~/.cache/naleystogramm/plugins/
    [[nodiscard]] static QString pluginsDir();

    // Папка с распакованными .so: ~/.cache/naleystogramm/plugins/.extracted/<id>/
    [[nodiscard]] static QString extractedDir(const QString& pluginId);

    // Суммарный CSS от включённых плагинов (вызывается из ThemeManager)
    [[nodiscard]] QString collectExtraCSS() const;

signals:
    void pluginsChanged();
    void reloaded();

private:
    explicit PluginManager(QObject* parent = nullptr);

    // Загружает .so из папки extracted для указанного entry
    bool loadEntry(PluginEntry& entry);

    void loadEnabledState();
    void saveEnabledState() const;

    // Извлекает .so из .plugin и загружает через QPluginLoader
    bool    extractAndLoad(PluginEntry& entry, const QString& key);
    // Путь к .so в директории распаковки
    static QString nativeSoPath(const QString& dir);
    // Путь к .plugin файлу по id
    QString pluginFilePath(const QString& id) const;
    // Строковое представление состояния для лога
    static QString stateToStr(PluginState s);

    QList<PluginEntry>  m_plugins;
    QMap<QString, bool> m_enabledState;  // id → enabled
    QMap<QString, QString> m_keys;       // id → key (in-memory only, not persisted)
    PluginHost          m_host;
    bool                m_loaded {false};
};
