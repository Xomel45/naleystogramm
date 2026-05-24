#pragma once
#include <QWidget>
#include <QPixmap>

class QLabel;
class QPropertyAnimation;
class QPushButton;

class SideDrawer : public QWidget {
    Q_OBJECT
public:
    explicit SideDrawer(QWidget* parent);

    void open(const QString& name, const QPixmap& avatar);
    void closeDrawer();
    void closeInstant();  // скрыть без анимации (для чистого захвата фона)
    bool isOpen() const { return m_open; }

signals:
    void editNameRequested();
    void showIdRequested();
    void addContactRequested();
    void settingsRequested();
    void closed();  // испускается когда анимация закрытия завершилась

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;

private:
    void buildUi();
    void applyTheme();
    void positionFooter();
    QPushButton* makeRow(const QString& iconPath, const QString& text);

    QLabel*             m_avatarLabel {nullptr};
    QLabel*             m_nameLabel   {nullptr};
    QPropertyAnimation* m_anim        {nullptr};
    QWidget*            m_footer      {nullptr};
    bool                m_open        {false};
};
