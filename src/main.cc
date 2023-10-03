/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT
 *<http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "AppMessages.h"
#include "LoginCheck.h"
#include "QGC.h"
#include "QGCApplication.h"
#include "DBConnect.h"
#include "User.h"
#include <QApplication>
#include <QHostAddress>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QSslSocket>
#include <QStringListModel>
#include <QUdpSocket>
#include <QVBoxLayout>
#include <QtCore/qlogging.h>
#include <QtPlugin>
#include <QtWidgets/qdialog.h>
#include <chrono>
#include <thread>

#ifndef NO_SERIAL_LINK

#include "SerialLink.h"

#endif

#ifndef __mobile__

#include "QGCSerialPortInfo.h"
#include "RunGuard.h"

#ifndef NO_SERIAL_LINK

#include <QSerialPort>

#endif
#endif

#ifdef UNITTEST_BUILD

#include "UnitTest.h"

#endif

#ifdef QT_DEBUG

#include "CmdLineOptParser.h"

#ifdef Q_OS_WIN
#include <crtdbg.h>
#endif
#endif

#ifdef QGC_ENABLE_BLUETOOTH
#include <QtBluetooth/QBluetoothSocket>
#endif

#include "QGCMapEngine.h"
#include <chrono>
#include <iostream>
#include <thread>

/* SDL does ugly things to main() */
#ifdef main
#undef main
#endif

#ifndef __mobile__
#ifndef NO_SERIAL_LINK

Q_DECLARE_METATYPE(QGCSerialPortInfo)

#endif
#endif

#ifdef Q_OS_WIN

#include <windows.h>

/// @brief CRT Report Hook installed using _CrtSetReportHook. We install this
/// hook when we don't want asserts to pop a dialog on windows.
int WindowsCrtReportHook(int reportType, char *message, int *returnValue) {
  Q_UNUSED(reportType);

  std::cerr << message << std::endl; // Output message to stderr
  *returnValue = 0;                  // Don't break into debugger
  return true;                       // We handled this fully ourselves
}

#endif

#if defined(__android__)
#include "JoystickAndroid.h"
#include <jni.h>
#if defined(QGC_ENABLE_PAIRING)
#include "PairingManager.h"
#endif
#if !defined(NO_SERIAL_LINK)
#include "qserialport.h"
#endif

static jobject _class_loader = nullptr;
static jobject _context = nullptr;

//-----------------------------------------------------------------------------
extern "C" {
void gst_amc_jni_set_java_vm(JavaVM *java_vm);

jobject gst_android_get_application_class_loader(void) { return _class_loader; }
}

//-----------------------------------------------------------------------------
static void gst_android_init(JNIEnv *env, jobject context) {
  jobject class_loader = nullptr;

  jclass context_cls = env->GetObjectClass(context);
  if (!context_cls) {
    return;
  }

  jmethodID get_class_loader_id = env->GetMethodID(
      context_cls, "getClassLoader", "()Ljava/lang/ClassLoader;");
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return;
  }

  class_loader = env->CallObjectMethod(context, get_class_loader_id);
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return;
  }

  _context = env->NewGlobalRef(context);
  _class_loader = env->NewGlobalRef(class_loader);
}

//-----------------------------------------------------------------------------
static const char kJniClassName[]{"org/mavlink/qgroundcontrol/QGCActivity"};

void setNativeMethods(void) {
  JNINativeMethod javaMethods[]{
      {"nativeInit", "()V", reinterpret_cast<void *>(gst_android_init)}};

  QAndroidJniEnvironment jniEnv;
  if (jniEnv->ExceptionCheck()) {
    jniEnv->ExceptionDescribe();
    jniEnv->ExceptionClear();
  }

  jclass objectClass = jniEnv->FindClass(kJniClassName);
  if (!objectClass) {
    qWarning() << "Couldn't find class:" << kJniClassName;
    return;
  }

  jint val = jniEnv->RegisterNatives(
      objectClass, javaMethods, sizeof(javaMethods) / sizeof(javaMethods[0]));

  if (val < 0) {
    qWarning() << "Error registering methods: " << val;
  } else {
    qDebug() << "Main Native Functions Registered";
  }

  if (jniEnv->ExceptionCheck()) {
    jniEnv->ExceptionDescribe();
    jniEnv->ExceptionClear();
  }
}

//-----------------------------------------------------------------------------
jint JNI_OnLoad(JavaVM *vm, void *reserved) {
  Q_UNUSED(reserved);

  JNIEnv *env;
  if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return -1;
  }

  setNativeMethods();

#if defined(QGC_GST_STREAMING)
  // Tell the androidmedia plugin about the Java VM
  gst_amc_jni_set_java_vm(vm);
#endif

#if !defined(NO_SERIAL_LINK)
  QSerialPort::setNativeMethods();
#endif

  JoystickAndroid::setNativeMethods();

#if defined(QGC_ENABLE_PAIRING)
  PairingManager::setNativeMethods();
#endif

  return JNI_VERSION_1_6;
}
#endif

//-----------------------------------------------------------------------------
#ifdef __android__
#include <QtAndroid>
bool checkAndroidWritePermission() {
  QtAndroid::PermissionResult r =
      QtAndroid::checkPermission("android.permission.WRITE_EXTERNAL_STORAGE");
  if (r == QtAndroid::PermissionResult::Denied) {
    QtAndroid::requestPermissionsSync(
        QStringList() << "android.permission.WRITE_EXTERNAL_STORAGE");
    r = QtAndroid::checkPermission("android.permission.WRITE_EXTERNAL_STORAGE");
    if (r == QtAndroid::PermissionResult::Denied) {
      return false;
    }
  }
  return true;
}
#endif

// To shut down QGC on Ctrl+C on Linux
#ifdef Q_OS_LINUX

#include <csignal>

void sigHandler(int s) {
    std::signal(s, SIG_DFL);
    QApplication::instance()->quit();
}

#endif /* Q_OS_LINUX */

//-----------------------------------------------------------------------------
/**
 * @brief Starts the application
 *
 * @param argc Number of commandline arguments
 * @param argv Commandline arguments
 * @return exit code, 0 for normal exit and !=0 for error cases
 */

int main(int argc, char *argv[]) {
#ifndef __mobile__
    // We make the runguard key different for custom and non custom
    // builds, so they can be executed together in the same device.
    // Stable and Daily have same QGC_APPLICATION_NAME so they would
    // not be able to run at the same time
    QString runguardString(QGC_APPLICATION_NAME);
    runguardString.append("RunGuardKey");

    RunGuard guard(runguardString);
    if (!guard.tryToRun()) {
        // QApplication is necessary to use QMessageBox
        QApplication errorApp(argc, argv);
        QMessageBox::critical(
                nullptr, QObject::tr("Error"),
                QObject::tr("A second instance of %1 is already running. Please close "
                            "the other instance and try again.")
                        .arg(QGC_APPLICATION_NAME));
        return -1;
    }
#endif

    //-- Record boot time
    QGC::initTimer();

#ifdef Q_OS_UNIX
    // Force writing to the console on UNIX/BSD devices
    if (!qEnvironmentVariableIsSet("QT_LOGGING_TO_CONSOLE"))
        qputenv("QT_LOGGING_TO_CONSOLE", "1");
#endif

    // install the message handler
    AppMessages::installHandler();

#ifdef Q_OS_MAC
#ifndef __ios__
    // Prevent Apple's app nap from screwing us over
    // tip: the domain can be cross-checked on the command line with <defaults
    // domains>
    QProcess::execute(
        "defaults",
        {"write org.qgroundcontrol.qgroundcontrol NSAppSleepDisabled -bool YES"});
#endif
#endif

#ifdef Q_OS_LINUX
    std::signal(SIGINT, sigHandler);
    std::signal(SIGTERM, sigHandler);
#endif /* Q_OS_LINUX */

    // The following calls to qRegisterMetaType are done to silence debug output
    // which warns that we use these types in signals, and without calling
    // qRegisterMetaType we can't queue these signals. In general we don't queue
    // these signals, but we do what the warning says anyway to silence the debug
    // output.
#ifndef NO_SERIAL_LINK
    qRegisterMetaType<QSerialPort::SerialPortError>();
#endif
#ifdef QGC_ENABLE_BLUETOOTH
    qRegisterMetaType<QBluetoothSocket::SocketError>();
    qRegisterMetaType<QBluetoothServiceInfo>();
#endif
    qRegisterMetaType<QAbstractSocket::SocketError>();
#ifndef __mobile__
#ifndef NO_SERIAL_LINK
    qRegisterMetaType<QGCSerialPortInfo>();
#endif
#endif

    qRegisterMetaType<Vehicle::MavCmdResultFailureCode_t>(
            "Vehicle::MavCmdResultFailureCode_t");

    // We statically link our own QtLocation plugin

#ifdef Q_OS_WIN
    // In Windows, the compiler doesn't see the use of the class created by
    // Q_IMPORT_PLUGIN
#pragma warning(disable : 4930 4101)
#endif

    Q_IMPORT_PLUGIN(QGeoServiceProviderFactoryQGC)

    bool runUnitTests = false; // Run unit tests
    bool flag = false;

    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QGCApplication *app = new QGCApplication(argc, argv, runUnitTests);
    Q_CHECK_PTR(app);
    if (app->isErrorState()) {
        app->exec();
        return -1;
    }

    QDialog dialog;
    DBConnect DB = DBConnect("localhost", "27017");

    auto *username = new QLineEdit;
    username->setPlaceholderText("Username");

    auto *password = new QLineEdit;
    password->setPlaceholderText("Password");
    password->setEchoMode(QLineEdit::Password);

    auto *loginButton = new QPushButton("Login");
    QObject::connect(loginButton, &QPushButton::clicked, [&]() {
        if (DB.LoginCheck(username->text().toStdString(),
                          password->text().toStdString())) {
            // QMessageBox::information(nullptr, "Information", "Login success");
            qDebug() << "Login success";
            flag = true;
            dialog.close();
        } else {
            QMessageBox::warning(nullptr, "Warning", "Wrong username or password");
            flag = false;
        }
    });

    auto *lable = new QLabel("Please Login");

    auto *layout = new QVBoxLayout;
    layout->addWidget(lable);
    layout->addWidget(username);
    layout->addWidget(password);
    layout->addWidget(loginButton);

    dialog.setLayout(layout);

    dialog.setWindowTitle("Login");
    dialog.resize(250, 100);

    dialog.show();
    dialog.exec();

    if (flag) {
        DB.UploadLog("/home/lab509/Documents/QGroundControl/CrashLogs");
#ifdef Q_OS_LINUX
        QApplication::setWindowIcon(
                QIcon(":/res/resources/icons/qgroundcontrol.ico"));
#endif /* Q_OS_LINUX */

        // There appears to be a threading issue in qRegisterMetaType which can
        // cause it to throw a qWarning about duplicate type converters. This is
        // caused by a race condition in the Qt code. Still working with them on
        // tracking down the bug. For now we register the type which is giving us
        // problems here while we only have the main thread. That should prevent it
        // from hitd_one_result = collection.find_one(make_ting the race condition later on in the code.
        qRegisterMetaType<QList<QPair<QByteArray, QByteArray>>>();

        app->_initCommon();
        //-- Initialize Cache System
        getQGCMapEngine()->init();

        // Add any branch here
        int exitCode = 0;

#ifdef __android__
        checkAndroidWritePermission();
#endif
        if (!app->_initForNormalAppBoot()) {
            return -1;
        }

        exitCode = app->exec();

        app->_shutdown();
        delete app;
        //-- Shutdown Cache System
        destroyMapEngine();

        qDebug() << "After app delete";

        return exitCode;
    } else {
        qDebug() << "Exit app without login";

        return -1;
    }
}
