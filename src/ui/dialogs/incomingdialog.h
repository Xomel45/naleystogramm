#pragma once
#include <QDialog>

// FIX: вынесен в отдельный файл — раньше был объявлен в addcontactdialog.h
class IncomingDialog : public QDialog {
    Q_OBJECT
public:
    IncomingDialog(const QString& name, const QString& ip,
                   QWidget* parent = nullptr);
};