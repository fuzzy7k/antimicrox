/* antimicro Gamepad to KB+M event mapper
 * Copyright (C) 2015 Travis Nickles <nickles.travis@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "addeditautoprofiledialog.h"
#include "ui_addeditautoprofiledialog.h"
#include "autoprofileinfo.h"
#include "inputdevice.h"
#include "antimicrosettings.h"
#include "common.h"

#if defined(Q_OS_UNIX)

    #ifdef WITH_X11
#include "unixcapturewindowutility.h"
#include "capturedwindowinfodialog.h"
#include "x11extras.h"

    #endif

#include <QApplication>


#elif defined(Q_OS_WIN)
#include "winappprofiletimerdialog.h"
#include "capturedwindowinfodialog.h"
#include "winextras.h"

#endif

#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QList>
#include <QListIterator>
#include <QMessageBox>
#include <QThread>



AddEditAutoProfileDialog::AddEditAutoProfileDialog(AutoProfileInfo *info, AntiMicroSettings *settings,
                                                   QList<InputDevice*> *devices,
                                                   QList<QString> &reservedGUIDS, bool edit, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddEditAutoProfileDialog)
{
    ui->setupUi(this);

    qDebug() << "[" << __FILE__ << ": " << __LINE__ << "] " << __FUNCTION__;
    setAttribute(Qt::WA_DeleteOnClose);

    this->info = info;
    this->settings = settings;
    this->editForm = edit;
    this->devices = devices;
    this->originalGUID = info->getGUID();
    this->originalExe = info->getExe();
    this->originalWindowClass = info->getWindowClass();
    this->originalWindowName = info->getWindowName();

    QListIterator<QString> iterGUIDs(reservedGUIDS);
    while (iterGUIDs.hasNext())
    {
        QString guid = iterGUIDs.next();
        if (!this->reservedGUIDs.contains(guid))
        {
            this->reservedGUIDs.append(guid);
        }
    }

    bool allowDefault = false;
    if ((info->getGUID() != "all") &&
        (info->getGUID() != "") &&
        !this->reservedGUIDs.contains(info->getGUID()))
    {
        allowDefault = true;
    }

    if (allowDefault && info->getExe().isEmpty())
    {
        ui->asDefaultCheckBox->setEnabled(true);
        if (info->isCurrentDefault())
        {
            ui->asDefaultCheckBox->setChecked(true);
        }
    }
    else
    {
        ui->asDefaultCheckBox->setToolTip(trUtf8("A different profile is already selected as the default for this device."));
    }

        ui->devicesComboBox->addItem("all");
        QListIterator<InputDevice*> iter(*devices);
        int found = -1;
        int numItems = 1;
        while (iter.hasNext())
        {
            InputDevice *device = iter.next();
            ui->devicesComboBox->addItem(device->getSDLName(), QVariant::fromValue<InputDevice*>(device));
            if (device->getGUIDString() == info->getGUID())
            {
                found = numItems;
            }
            numItems++;
        }

        if (!info->getGUID().isEmpty() && (info->getGUID() != "all"))
        {
            if (found >= 0)
            {
                ui->devicesComboBox->setCurrentIndex(found);
            }
            else
            {
                ui->devicesComboBox->addItem(trUtf8("Current (%1)").arg(info->getDeviceName()));
                ui->devicesComboBox->setCurrentIndex(ui->devicesComboBox->count()-1);
            }
        }

    ui->profileLineEdit->setText(info->getProfileLocation());
    ui->applicationLineEdit->setText(info->getExe());
    ui->winClassLineEdit->setText(info->getWindowClass());
    ui->winNameLineEdit->setText(info->getWindowName());

#ifdef Q_OS_UNIX
    ui->selectWindowPushButton->setVisible(false);

#elif defined(Q_OS_WIN)
    ui->detectWinPropsSelectWindowPushButton->setVisible(false);

    ui->winClassLineEdit->setVisible(false);
    ui->winClassLabel->setVisible(false);
#endif

    connect(ui->profileBrowsePushButton, SIGNAL(clicked()), this, SLOT(openProfileBrowseDialog()));
    connect(ui->applicationPushButton, SIGNAL(clicked()), this, SLOT(openApplicationBrowseDialog()));
    connect(ui->devicesComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(checkForReservedGUIDs(int)));
    connect(ui->applicationLineEdit, SIGNAL(textChanged(QString)), this, SLOT(checkForDefaultStatus()));
    connect(ui->winClassLineEdit, SIGNAL(textChanged(QString)), this, SLOT(checkForDefaultStatus()));
    connect(ui->winNameLineEdit, SIGNAL(textChanged(QString)), this, SLOT(checkForDefaultStatus()));

#if defined(Q_OS_UNIX)
    connect(ui->detectWinPropsSelectWindowPushButton, SIGNAL(clicked()), this, SLOT(showCaptureHelpWindow()));
#elif defined(Q_OS_WIN)
    connect(ui->selectWindowPushButton, SIGNAL(clicked()), this, SLOT(openWinAppProfileDialog()));
#endif

    connect(this, SIGNAL(accepted()), this, SLOT(saveAutoProfileInformation()));
}

AddEditAutoProfileDialog::~AddEditAutoProfileDialog()
{
    qDebug() << "[" << __FILE__ << ": " << __LINE__ << "] " << __FUNCTION__;

    delete ui;
}

void AddEditAutoProfileDialog::openProfileBrowseDialog()
{
    qDebug() << "[" << __FILE__ << ": " << __LINE__ << "] " << __FUNCTION__;

    QString lookupDir = PadderCommon::preferredProfileDir(settings);
    QString filename = QFileDialog::getOpenFileName(this, trUtf8("Open Config"), lookupDir, QString("Config Files (*.amgp *.xml)"));
    if (!filename.isNull() && !filename.isEmpty())
    {
        ui->profileLineEdit->setText(QDir::toNativeSeparators(filename));
    }
}

void AddEditAutoProfileDialog::openApplicationBrowseDialog()
{
    qDebug() << "[" << __FILE__ << ": " << __LINE__ << "] " << __FUNCTION__;

#ifdef Q_OS_WIN
    QString filename = QFileDialog::getOpenFileName(this, trUtf8("Select Program"), QDir::homePath(), trUtf8("Programs (*.exe)"));
#elif defined(Q_OS_LINUX)
    QString filename = QFileDialog::getOpenFileName(this, trUtf8("Select Program"), QDir::homePath(), QString());
#endif
    if (!filename.isNull() && !filename.isEmpty())
    {
        QFileInfo exe(filename);
        if (exe.exists() && exe.isExecutable())
        {
            ui->applicationLineEdit->setText(filename);
        }
    }
}

AutoProfileInfo* AddEditAutoProfileDialog::getAutoProfile() const
{
    qDebug() << "[" << __FILE__ << ": " << __LINE__ << "] " << __FUNCTION__;

    return info;
}

void AddEditAutoProfileDialog::saveAutoProfileInformation()
{
    qDebug() << "[" << __FILE__ << ": " << __LINE__ << "] " << __FUNCTION__;

    info->setProfileLocation(ui->profileLineEdit->text());
    int deviceIndex = ui->devicesComboBox->currentIndex();
    if (deviceIndex > 0)
    {
        QVariant temp = ui->devicesComboBox->itemData(deviceIndex, Qt::UserRole);
        // Assume that if the following is not true, the GUID should
        // not be changed.
        if (!temp.isNull())
        {
            InputDevice *device = ui->devicesComboBox->itemData(deviceIndex, Qt::UserRole).value<InputDevice*>();
            info->setGUID(device->getGUIDString());
            info->setDeviceName(device->getSDLName());
        }
    }
    else
    {
        info->setGUID("all");
        info->setDeviceName("");
    }

    info->setExe(ui->applicationLineEdit->text());
    info->setWindowClass(ui->winClassLineEdit->text());
    info->setWindowName(ui->winNameLineEdit->text());
    info->setDefaultState(ui->asDefaultCheckBox->isChecked());
}

void AddEditAutoProfileDialog::checkForReservedGUIDs(int index)
{
    qDebug() << "[" << __FILE__ << ": " << __LINE__ << "] " << __FUNCTION__;

    QVariant data = ui->devicesComboBox->itemData(index);
    if (index == 0)
    {
        ui->asDefaultCheckBox->setChecked(false);
        ui->asDefaultCheckBox->setEnabled(false);
        ui->asDefaultCheckBox->setToolTip(trUtf8("Please use the main default profile selection."));
    }
    else if (!data.isNull())
    {
        InputDevice *device = data.value<InputDevice*>();
        if (reservedGUIDs.contains(device->getGUIDString()))
        {
            ui->asDefaultCheckBox->setChecked(false);
            ui->asDefaultCheckBox->setEnabled(false);
            ui->asDefaultCheckBox->setToolTip(trUtf8("A different profile is already selected as the default for this device."));
        }
        else
        {
            ui->asDefaultCheckBox->setEnabled(true);
            ui->asDefaultCheckBox->setToolTip(trUtf8("Select this profile to be the default loaded for\nthe specified device. The selection will be used instead\nof the all default profile option."));
        }
    }
}

QString AddEditAutoProfileDialog::getOriginalGUID() const
{
    qDebug() << "[" << __FILE__ << ": " << __LINE__ << "] " << __FUNCTION__;

    return originalGUID;
}

QString AddEditAutoProfileDialog::getOriginalExe() const
{
    qDebug() << "[" << __FILE__ << ": " << __LINE__ << "] " << __FUNCTION__;

    return originalExe;
}

QString AddEditAutoProfileDialog::getOriginalWindowClass() const
{
    qDebug() << "[" << __FILE__ << ": " << __LINE__ << "] " << __FUNCTION__;

    return originalWindowClass;
}

QString AddEditAutoProfileDialog::getOriginalWindowName() const
{
    qDebug() << "[" << __FILE__ << ": " << __LINE__ << "] " << __FUNCTION__;

    return originalWindowName;
}

#ifdef Q_OS_UNIX
/**
 * @brief Display a simple message box and attempt to capture a window using the mouse
 */
void AddEditAutoProfileDialog::showCaptureHelpWindow()
{
    qDebug() << "[" << __FILE__ << ": " << __LINE__ << "] " << __FUNCTION__;
    #ifdef WITH_X11

    if (QApplication::platformName() == QStringLiteral("xcb"))
    {
    QMessageBox *box = new QMessageBox(this);
    box->setText(trUtf8("Please select a window by using the mouse. Press Escape if you want to cancel."));
    box->setWindowTitle(trUtf8("Capture Application Window"));
    box->setStandardButtons(QMessageBox::NoButton);
    box->setModal(true);
    box->show();

    UnixCaptureWindowUtility *util = new UnixCaptureWindowUtility();
    QThread *thread = new QThread; // QTHREAD(this)
    util->moveToThread(thread);

    connect(thread, SIGNAL(started()), util, SLOT(attemptWindowCapture()));
    connect(util, SIGNAL(captureFinished()), thread, SLOT(quit()));
    connect(util, SIGNAL(captureFinished()), box, SLOT(hide()));
    connect(util, SIGNAL(captureFinished()), this, SLOT(checkForGrabbedWindow()), Qt::QueuedConnection);

    connect(thread, SIGNAL(finished()), box, SLOT(deleteLater()));
    connect(util, SIGNAL(destroyed()), thread, SLOT(deleteLater()));
    thread->start();
    }

    #endif
}

/**
 * @brief Check if there is a program path saved in an UnixCaptureWindowUtility
 *     object
 */
void AddEditAutoProfileDialog::checkForGrabbedWindow()
{
    qDebug() << "[" << __FILE__ << ": " << __LINE__ << "] " << __FUNCTION__;

    #ifdef WITH_X11
    if (QApplication::platformName() == QStringLiteral("xcb"))
    {
    UnixCaptureWindowUtility *util = qobject_cast<UnixCaptureWindowUtility*>(sender()); // static_cast
    long targetWindow = util->getTargetWindow();
    bool escaped = !util->hasFailed();
    bool failed = false;
    QString path = QString();

    if (targetWindow != None)
    {
        // Attempt to find the appropriate window below the root window
        // that was clicked.
        qDebug() << "ORIGINAL: " << QString::number(targetWindow, 16);
        long tempWindow = X11Extras::getInstance()->findClientWindow(targetWindow);
        if (tempWindow > 0)
        {
            targetWindow = tempWindow;
        }
        qDebug() << "ADJUSTED: " << QString::number(targetWindow, 16);
    }

    if (targetWindow != None)
    {
        CapturedWindowInfoDialog *dialog = new CapturedWindowInfoDialog(targetWindow, this);
        connect(dialog, SIGNAL(accepted()), this, SLOT(windowPropAssignment()));
        dialog->show();
    }
    else if (!escaped)
    {
        failed = true;
    }

    // Ensure that the operation was not cancelled (Escape wasn't pressed).
    if (failed)
    {
        QMessageBox box;
        box.setText(trUtf8("Could not obtain information for the selected window."));
        box.setWindowTitle(trUtf8("Application Capture Failed"));
        box.setStandardButtons(QMessageBox::Close);
        box.raise();
        box.exec();
    }

    util->deleteLater();
    }
    #endif

}

#endif

void AddEditAutoProfileDialog::windowPropAssignment()
{
    qDebug() << "[" << __FILE__ << ": " << __LINE__ << "] " << __FUNCTION__;

    disconnect(ui->applicationLineEdit, SIGNAL(textChanged(QString)), this, SLOT(checkForDefaultStatus()));
    disconnect(ui->winClassLineEdit, SIGNAL(textChanged(QString)), this, SLOT(checkForDefaultStatus()));
    disconnect(ui->winNameLineEdit, SIGNAL(textChanged(QString)), this, SLOT(checkForDefaultStatus()));

    CapturedWindowInfoDialog *dialog = qobject_cast<CapturedWindowInfoDialog*>(sender()); // static_cast
    if (dialog->getSelectedOptions() & CapturedWindowInfoDialog::WindowPath)
    {
        if (dialog->useFullWindowPath())
        {
            ui->applicationLineEdit->setText(dialog->getWindowPath());
        }
        else
        {
            QString temp = QString();
            temp = QFileInfo(dialog->getWindowPath()).fileName();
            ui->applicationLineEdit->setText(temp);
        }
    }
    else
    {
        ui->applicationLineEdit->clear();
    }

    if (dialog->getSelectedOptions() & CapturedWindowInfoDialog::WindowClass)
    {
        ui->winClassLineEdit->setText(dialog->getWindowClass());
    }
    else
    {
        ui->winClassLineEdit->clear();
    }

    if (dialog->getSelectedOptions() & CapturedWindowInfoDialog::WindowName)
    {
        ui->winNameLineEdit->setText(dialog->getWindowName());
    }
    else
    {
        ui->winNameLineEdit->clear();
    }

    checkForDefaultStatus();

    connect(ui->applicationLineEdit, SIGNAL(textChanged(QString)), this, SLOT(checkForDefaultStatus()));
    connect(ui->winClassLineEdit, SIGNAL(textChanged(QString)), this, SLOT(checkForDefaultStatus()));
    connect(ui->winNameLineEdit, SIGNAL(textChanged(QString)), this, SLOT(checkForDefaultStatus()));
}


void AddEditAutoProfileDialog::checkForDefaultStatus()
{
    qDebug() << "[" << __FILE__ << ": " << __LINE__ << "] " << __FUNCTION__;

    bool status = ui->applicationLineEdit->text().length() > 0;
    status = status ? status : ui->winClassLineEdit->text().length() > 0;
    status = status ? status : ui->winNameLineEdit->text().length() > 0;

    if (status)
    {
        ui->asDefaultCheckBox->setChecked(false);
        ui->asDefaultCheckBox->setEnabled(false);
    }
    else
    {
        ui->asDefaultCheckBox->setEnabled(true);
    }
}

/**
 * @brief Validate the form that is contained in this window
 */
void AddEditAutoProfileDialog::accept()
{
    qDebug() << "[" << __FILE__ << ": " << __LINE__ << "] " << __FUNCTION__;

    bool validForm = true;
    bool propertyFound = false;

    QString errorString = QString();
    if (ui->profileLineEdit->text().length() > 0)
    {
        QString profileFilename = ui->profileLineEdit->text();
        QFileInfo info(profileFilename);
        if (!info.exists())
        {
            validForm = false;
            errorString = trUtf8("Profile file path is invalid.");
        }
    }

    if (validForm &&
        (ui->applicationLineEdit->text().isEmpty() &&
         ui->winClassLineEdit->text().isEmpty() &&
         ui->winNameLineEdit->text().isEmpty()))
    {
        validForm = false;
        errorString = trUtf8("No window matching property was specified.");
    }
    else
    {
        propertyFound = true;
    }

    if (validForm && !ui->applicationLineEdit->text().isEmpty())
    {
        QString exeFileName = ui->applicationLineEdit->text();
        QFileInfo info(exeFileName);
        if (info.isAbsolute() && (!info.exists() || !info.isExecutable()))
        {
            validForm = false;
            errorString = trUtf8("Program path is invalid or not executable.");
        }
#ifdef Q_OS_WIN
        else if (!info.isAbsolute() &&
                 ((info.fileName() != exeFileName) ||
                  (info.suffix() != "exe")))
        {
            validForm = false;
            errorString = trUtf8("File is not an .exe file.");
        }
#endif
    }

    if (validForm && !propertyFound && !ui->asDefaultCheckBox->isChecked())
    {
        validForm = false;
        errorString = trUtf8("No window matching property was selected.");
    }

    if (validForm)
    {
        QDialog::accept();
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setText(errorString);
        msgBox.setStandardButtons(QMessageBox::Close);
        msgBox.exec();
    }
}

#ifdef Q_OS_WIN
void AddEditAutoProfileDialog::openWinAppProfileDialog()
{
    qDebug() << "[" << __FILE__ << ": " << __LINE__ << "] " << __FUNCTION__;

    WinAppProfileTimerDialog *dialog = new WinAppProfileTimerDialog(this);
    connect(dialog, SIGNAL(accepted()), this, SLOT(captureWindowsApplicationPath()));
    dialog->show();
}

void AddEditAutoProfileDialog::captureWindowsApplicationPath()
{
    qDebug() << "[" << __FILE__ << ": " << __LINE__ << "] " << __FUNCTION__;

    CapturedWindowInfoDialog *dialog = new CapturedWindowInfoDialog(this);
    connect(dialog, SIGNAL(accepted()), this, SLOT(windowPropAssignment()));
    dialog->show();

}

#endif

QList<InputDevice*> *AddEditAutoProfileDialog::getDevices() const {

    return devices;
}

AntiMicroSettings *AddEditAutoProfileDialog::getSettings() const {

    return settings;
}

bool AddEditAutoProfileDialog::getEditForm() const {

    return editForm;
}

bool AddEditAutoProfileDialog::getDefaultInfo() const {

    return defaultInfo;
}

QList<QString> const& AddEditAutoProfileDialog::getReservedGUIDs() {

    return reservedGUIDs;
}
