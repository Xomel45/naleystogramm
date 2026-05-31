#pragma once
#include <QtPlugin>
#include <QString>

class QWidget;

// ── IPluginHost ───────────────────────────────────────────────────────────
// Интерфейс приложения, предоставляемый плагину через initialize().

class IPluginHost {
public:
    virtual ~IPluginHost() = default;

    virtual void    log(const QString& msg) = 0;
    virtual QString appVersion()  const = 0;
    virtual QString pluginsDir()  const = 0;
};

// ── IPlugin ───────────────────────────────────────────────────────────────
// Интерфейс плагина. Реализуется в разделяемой библиотеке (.so / .dll).
//
// Минимальный плагин:
//
//   class MyPlugin : public QObject, public IPlugin {
//       Q_OBJECT
//       Q_INTERFACES(IPlugin)
//       Q_PLUGIN_METADATA(IID "ru.naleystogramm.IPlugin/1.0")
//   public:
//       QString id()          const override { return "com.example.myplugin"; }
//       QString name()        const override { return "My Plugin"; }
//       QString description() const override { return "Does something cool"; }
//       QString version()     const override { return "1.0.0"; }
//   };

class IPlugin {
public:
    virtual ~IPlugin() = default;

    virtual QString id()          const = 0;   // уникальный ID (reverse-dns)
    virtual QString name()        const = 0;   // отображаемое название
    virtual QString description() const = 0;   // краткое описание
    virtual QString version()     const = 0;   // версия плагина

    // Вызывается при загрузке плагина (до первого отображения UI).
    virtual void initialize(IPluginHost* host) { Q_UNUSED(host) }

    // Вызывается перед выгрузкой плагина.
    virtual void shutdown() {}

    // Опциональная страница настроек плагина. Caller takes ownership.
    // Возвращает nullptr если плагин не имеет настроек.
    virtual QWidget* createSettingsPage() { return nullptr; }
    virtual QString  settingsPageTitle()  const { return name(); }

    // Опциональный CSS — добавляется к стилю приложения при активации плагина.
    // Обновляется автоматически при включении/выключении плагина.
    virtual QString  extraStyleSheet()    const { return {}; }
};

#define NaleystogrammPlugin_iid "ru.naleystogramm.IPlugin/1.0"
Q_DECLARE_INTERFACE(IPlugin, NaleystogrammPlugin_iid)
