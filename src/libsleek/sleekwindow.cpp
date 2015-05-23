#include "sleekwindow.h"
#include <QPushButton>
#include <QDebug>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#include <stdexcept>
#include <dwmapi.h>
#include <windowsx.h>
#include <QWindow>
#include <QApplication>
#include "sleekwindowclass.h"

SleekWindow::SleekWindow(QApplication *app, QString& title, bool isMainWindow) :
    _mainPanel(new QWidget),
    _parenthWnd(0),
    _hWnd(0),
    _borderless( false ),
    _borderlessResizeable( true ),
    _aeroShadow( false ),
    _closed( false ),
    _visible( false ),
    _isFirstTime ( true ),
    _app(app),
    _isMainWindow ( isMainWindow ),
    _result(false)
{
    init(title);
}

SleekWindow::SleekWindow(QApplication *app, QString &title, SleekWindow *parent) :
    _mainPanel(new QWidget),
    _parenthWnd(0),
    _hWnd(0),
    _borderless( false ),
    _borderlessResizeable( true ),
    _aeroShadow( false ),
    _closed( false ),
    _visible( false ),
    _isFirstTime ( true ),
    _app(app),
    _isMainWindow ( false ),
    _result(false)
{
    if (parent)
        _parenthWnd = parent->getHandle();

    init(title);
    EnableWindow(_parenthWnd, FALSE);

    if (parent)
    {
        toggleMinimize();
        toggleResizeable();
    }
}

SleekWindow::~SleekWindow()
{
    //_sleekBorderless->close();
    DestroyWindow(_hWnd);
    delete _sleekBorderless;
    qDebug() << "SleekWindow: DESTRUCT";
}

void SleekWindow::init(QString& title)
{
    _mainPanel->setStyleSheet("border:none;");
    _hWnd = CreateWindow(
                L"SleekWindowClass",
                title.toStdWString().c_str(),
                static_cast<DWORD>( Style::windowed ),
                0, 0, 0, 0,
                _parenthWnd,
                0,
                SleekWindowClass::Instance().getHInstance(),
                nullptr );

    if ( !_hWnd ) throw std::runtime_error( "Create window failed." );

    SetWindowLongPtr( _hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );

    _sleekBorderless = new SleekBorderless(_hWnd, _mainPanel);

    setupSignals();
}

void SleekWindow::setupSignals()
{
    connect(_sleekBorderless, SIGNAL(closing()), this, SLOT(slot_closing()));
}

void SleekWindow::centerPrimaryScreen()
{
    int xPos = (GetSystemMetrics(SM_CXSCREEN) - getMainPanel()->width())/2;
    int yPos = (GetSystemMetrics(SM_CYSCREEN) - getMainPanel()->height())/2;

    SetWindowPos( _hWnd, 0, xPos, yPos, 0, 0, SWP_NOZORDER | SWP_NOSIZE );
}

void SleekWindow::centerParent()
{
    if (_parenthWnd == 0)
        return;

    RECT rectParent;
    GetWindowRect(_parenthWnd, &rectParent);

    int parentWidth = rectParent.right - rectParent.left;
    int parentHeight = rectParent.bottom - rectParent.top;
    //qDebug() << "parent width:" << parentWidth << "height:" << parentHeight;
    //qDebug() << "parent left:" << rectParent.left << "top:" << rectParent.top << "right:" << rectParent.right << "bottom:" << rectParent.bottom;;

    int width = getMainPanel()->width();
    int height = getMainPanel()->height();
    //qDebug() << "child width:" << width << "height:" << height;

    int x = rectParent.left + (parentWidth / 2) - width / 2;
    int y = rectParent.top + (parentHeight / 2) - height / 2;

    RECT screenRect;
    screenRect.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    screenRect.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    screenRect.right = screenRect.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    screenRect.bottom = screenRect.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);

    int screenWidth = screenRect.right - screenRect.left;
    int screenHeight = screenRect.bottom - screenRect.top;

    // make sure that the dialog box never moves outside of the screen
    if (x < screenRect.left) x = screenRect.left;
    if (y < 0) y = 0;
    if (x + width > screenWidth) x = screenWidth - width;
    if (y + height > screenHeight) y = screenHeight - height;

    SetWindowPos( _hWnd, 0, x, y, 0, 0, SWP_FRAMECHANGED | SWP_NOSIZE );
}

HWND SleekWindow::getHandle()
{
    return _hWnd;
}

void SleekWindow::toggleBorderless()
{
    if (_visible)
    {
        _borderless = !_borderless;

        Style newStyle;
        if (_borderless && _borderlessResizeable)
        {
            newStyle = Style::aero_borderless;
        }
        else if (_borderless)
        {
            newStyle = Style::aero_borderless_no_resize;
        }
        else
        {
            newStyle = Style::windowed;
        }

        SetWindowLongPtr( _hWnd, GWL_STYLE, static_cast<LONG>( newStyle ) );
        if ( _borderless ) {
            toggleShadow();
        }

        //redraw frame
        SetWindowPos( _hWnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE );
        show();
    }
}

void SleekWindow::toggleShadow() {
    if ( _borderless ) {
        _aeroShadow = !_aeroShadow;
        const MARGINS shadow_on = { 1, 1, 1, 1 };
        const MARGINS shadow_off = { 0, 0, 0, 0 };
        DwmExtendFrameIntoClientArea( _hWnd, ( _aeroShadow ) ? ( &shadow_on ) : ( &shadow_off ) );
    }
}

void SleekWindow::toggleResizeable() {
    _borderlessResizeable = !_borderlessResizeable;

    QPushButton *pushButtonMaximize = _sleekBorderless->findChild<QPushButton*>( "pushButtonMaximize" );
    if (_borderlessResizeable)
    {
        Style newStyle = Style::aero_borderless;
        SetWindowLongPtr( _hWnd, GWL_STYLE, static_cast<LONG>( newStyle ) );
        SetWindowPos( _hWnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE );
        if (_visible)
            show();

        pushButtonMaximize->setVisible(true);
        _sleekBorderless->setResizeable(true);
        removeMaximumSize();
    }
    else
    {
        Style newStyle = Style::aero_borderless_no_resize;
        SetWindowLongPtr( _hWnd, GWL_STYLE, static_cast<LONG>( newStyle ) );
        SetWindowPos( _hWnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE );
        if (_visible)
            show();

        pushButtonMaximize->setVisible(false);
        _sleekBorderless->setResizeable(false);
        setMaximumSize(_mainPanel->width(), _mainPanel->height());
    }
}

void SleekWindow::toggleMinimize()
{
    QPushButton *pushButtonMinimize = _sleekBorderless->findChild<QPushButton*>( "pushButtonMinimize" );
    pushButtonMinimize->setVisible(!pushButtonMinimize->isVisible());
}

bool SleekWindow::isResizeable()
{
    return _borderlessResizeable ? true : false;
}
void SleekWindow::setResult(bool result)
{
    _result = result;
}

void SleekWindow::show()
{
    if (_isFirstTime)
    {
        _isFirstTime = false;
        setSize(_mainPanel->width(), _mainPanel->height());
    }

    if (_visible)
        ShowWindow( _hWnd, SW_SHOW );
    else
    {
        ShowWindow( _hWnd, SW_SHOW );
        _visible = true;
        toggleBorderless();
    }
}

bool SleekWindow::exec()
{
    show();
    _eventLoop.exec(QEventLoop::DialogExec);
    return _result;
}

void SleekWindow::slot_closing()
{
    close();
}

void SleekWindow::close()
{
    qDebug() << "CLOSING!";
    emit closing();
    _closed = true;
    _eventLoop.exit();
    EnableWindow(_parenthWnd, TRUE);
    hide();
    if (_isMainWindow)
        _app->quit();
    //DestroyWindow(_hWnd);
    //_sleekBorderless->close();
}

void SleekWindow::hide() {
    ShowWindow( _hWnd, SW_HIDE );
    _visible = false;
}

bool SleekWindow::isVisible() {
    return _visible ? true : false;
}

void SleekWindow::setSize(const int width, const int height)
{
    SetWindowPos(_hWnd, 0,0,0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// Minimum size
void SleekWindow::setMinimumSize( const int width, const int height ) {
    _minimumSize.required = true;
    _minimumSize.width = width;
    _minimumSize.height = height;
}

bool SleekWindow::isSetMinimumSize() {
    return _minimumSize.required;
}

void SleekWindow::removeMinimumSize() {
    _minimumSize.required = false;
    _minimumSize.width = 0;
    _minimumSize.height = 0;
}

int SleekWindow::getMinimumWidth() {
    return _minimumSize.width;
}

int SleekWindow::getMinimumHeight() {
    return _minimumSize.height;
}

// Maximum size
void SleekWindow::setMaximumSize( const int width, const int height ) {
    _maximumSize.required = true;
    _maximumSize.width = width;
    _maximumSize.height = height;
}

bool SleekWindow::isSetMaximumSize() {
    return _maximumSize.required;
}

void SleekWindow::removeMaximumSize() {
    _maximumSize.required = false;
    _maximumSize.width = 0;
    _maximumSize.height = 0;
}

int SleekWindow::getMaximumWidth() {
    return _maximumSize.width;
}

int SleekWindow::getMaximumHeight() {
    return _maximumSize.height;
}

bool SleekWindow::getBorderless()
{
    return _borderless;
}

bool SleekWindow::getBorderlessResizable()
{
    return _borderlessResizeable;
}

SleekBorderless* SleekWindow::getSleekBorderless()
{
    return _sleekBorderless;
}


#else
SleekWindow::SleekWindow(QApplication *app, QString title, bool isMainWindow) :
    _mainPanel(new QWidget()),
    _isMainWindow(isMainWindow),
    _result(false)
{
    Q_UNUSED(title);

    this->_app = app;
    _mainPanel->setObjectName("mainPanel");
}

SleekWindow::SleekWindow(QApplication *app, QString title, SleekWindow *parent) :
    _mainPanel(new QWidget()),
    _isMainWindow(false),
    _result(false)
{
    Q_UNUSED(title);
    Q_UNUSED(parent); //Should be used for modal window...

    //if (parent)
    //    _mainPanel = new QWidget(parent->getMainPanel());
    //else
    //    _mainPanel = new QWidget();

    this->_app = app;
    _mainPanel->setObjectName("mainPanel");
}

SleekWindow::~SleekWindow()
{

}

void SleekWindow::show()
{
    _mainPanel->show();
}

void SleekWindow::close()
{
    _eventLoop.exit();
    _mainPanel->close();
}

bool SleekWindow::exec()
{
    show();
    _eventLoop.exec();
    return _result;
}

void SleekWindow::setResult(bool result)
{
    _result = result;
}

void SleekWindow::toggleResizeable()
{
    _mainPanel->setFixedSize(_mainPanel->size());
}

void SleekWindow::setMinimumSize(const int width, const int height)
{
    _mainPanel->setMinimumSize(width, height);
}

void SleekWindow::setMaximumSize(const int width, const int height)
{
    _mainPanel->setMaximumSize(width, height);
}

void SleekWindow::centerParent()
{
    _mainPanel->move(_mainPanel->window()->frameGeometry().topLeft() +
        _mainPanel->window()->rect().center() -
        _mainPanel->window()->rect().center());
}

void SleekWindow::centerPrimaryScreen()
{
    _mainPanel->move(QApplication::desktop()->screen()->rect().center() - _mainPanel->rect().center());
}

#endif

QWidget *SleekWindow::getMainPanel()
{
    return _mainPanel;
}
