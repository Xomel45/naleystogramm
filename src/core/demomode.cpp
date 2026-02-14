#include "demomode.h"
#include "sessionmanager.h"

DemoMode& DemoMode::instance() {
    static DemoMode inst;
    return inst;
}

void DemoMode::setEnabled(bool on) {
    if (m_enabled == on) return;
    m_enabled = on;
    SessionManager::instance().setDemoMode(on);
    emit toggled(on);
}

QString DemoMode::displayName(const QString& real) const {
    return m_enabled ? kDemoName : real;
}

QString DemoMode::uuid(const QString& real) const {
    return m_enabled ? kDemoUuid : real;
}

QString DemoMode::ip(const QString& real) const {
    return m_enabled ? kDemoIp : real;
}

quint16 DemoMode::port(quint16 real) const {
    return m_enabled ? kDemoPort : real;
}
