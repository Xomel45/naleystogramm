#pragma once
#include <QWidget>
#include "../core/updatechecker.h"

class QLabel;
class QPushButton;

// ── UpdateBanner ───────────────────────────────────────────────────────────
// Тонкая полоска под хедером — появляется когда есть обновление.
// Скрыта по умолчанию.

class UpdateBanner : public QWidget {
    Q_OBJECT
public:
    explicit UpdateBanner(QWidget* parent = nullptr);
    void showUpdate(const UpdateInfo& info);
    void hide();

private:
    QLabel*      m_label  {nullptr};
    QPushButton* m_openBtn{nullptr};
    QPushButton* m_closeBtn{nullptr};
    QString      m_url;
};
