/*
 * Copyright (C) 2020 ~ 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     LiuMingHang <liuminghang@uniontech.com>
 *
 * Maintainer: ZhangYong <ZhangYong@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "viewpanel.h"

#include <QVBoxLayout>
#include <QShortcut>
#include <QFileInfo>
#include <DDesktopServices>
#include <DMenu>

#include "contents/bottomtoolbar.h"
#include "navigationwidget.h"
#include "unionimage/imageutils.h"
#include "unionimage/baseutils.h"
#include "imageengine.h"
#include "widgets/printhelper.h"
#include "contents/imageinfowidget.h"
#include "widgets/extensionpanel.h"
#include "widgets/toptoolbar.h"

const int BOTTOM_TOOLBAR_HEIGHT = 80;   //底部工具看高
const int BOTTOM_SPACING = 10;          //底部工具栏与底部边缘距离
const int RT_SPACING = 20;
const int TOP_TOOLBAR_HEIGHT = 50;

QString ss(const QString &text, const QString &defaultValue)
{
    Q_UNUSED(text);
    //采用代码中快捷键不使用配置文件快捷键
    // QString str = dApp->setter->value(SHORTCUTVIEW_GROUP, text, defaultValue).toString();
    QString str = defaultValue;
    str.replace(" ", "");
    return defaultValue;
}

ViewPanel::ViewPanel(QWidget *parent)
    : QFrame(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    this->setLayout(layout);
    m_stack = new DStackedWidget(this);
    layout->addWidget(m_stack);

    m_view = new ImageGraphicsView(this);
    m_stack->addWidget(m_view);

    m_bottomToolbar = new BottomToolbar(this);

    setContextMenuPolicy(Qt::CustomContextMenu);
    initConnect();
    initRightMenu();
    initFloatingComponent();
    initTopBar();
//    initExtensionPanel();
}

ViewPanel::~ViewPanel()
{

}

void ViewPanel::loadImage(const QString &path, QStringList paths)
{
    //展示图片
    m_view->setImage(path);
    m_view->resetTransform();
    m_stack->setCurrentWidget(m_view);
    //刷新工具栏
    m_bottomToolbar->setAllFile(path, paths);
    //重置底部工具栏位置与大小
    qDebug() << "---" << __FUNCTION__ << "---111111111111111";
    resetBottomToolbarGeometry(true);
}

void ViewPanel::initConnect()
{
    //缩略图列表，单机打开图片
    connect(m_bottomToolbar, &BottomToolbar::openImg, this, &ViewPanel::openImg);

    connect(m_view, &ImageGraphicsView::imageChanged, this, [ = ](QString path) {
        emit imageChanged(path);
        // Pixmap is cache in thread, make sure the size would correct after
        // cache is finish
        m_view->autoFit();
    });


    //旋转信号
    connect(m_bottomToolbar, &BottomToolbar::rotateClockwise, this, [ = ] {
        this->slotRotateImage(-90);

    });

    connect(m_bottomToolbar, &BottomToolbar::rotateCounterClockwise, this, [ = ] {
        this->slotRotateImage(90);
    });
}

void ViewPanel::initTopBar()
{
    m_topToolbar = new TopToolbar(false, this);
    m_topToolbar->resize(width(), 50);
    m_topToolbar->move(0, 0);
}

void ViewPanel::initFloatingComponent()
{
    initScaleLabel();
    initNavigation();
}

void ViewPanel::initScaleLabel()
{
    using namespace utils::base;
    DAnchors<DFloatingWidget> scalePerc = new DFloatingWidget(this);
    scalePerc->setBlurBackgroundEnabled(true);

    QHBoxLayout *layout = new QHBoxLayout();
    scalePerc->setLayout(layout);
    DLabel *label = new DLabel();
    layout->addWidget(label);
    scalePerc->setAttribute(Qt::WA_TransparentForMouseEvents);
    scalePerc.setAnchor(Qt::AnchorHorizontalCenter, this, Qt::AnchorHorizontalCenter);
    scalePerc.setAnchor(Qt::AnchorBottom, this, Qt::AnchorBottom);
    scalePerc.setBottomMargin(75 + 14);
    label->setAlignment(Qt::AlignCenter);
//    scalePerc->setFixedSize(82, 48);
    scalePerc->setFixedWidth(90 + 10);
    scalePerc->setFixedHeight(40 + 10);
    scalePerc->adjustSize();
    label->setText("100%");
    DFontSizeManager::instance()->bind(label, DFontSizeManager::T6);
    scalePerc->hide();

    QTimer *hideT = new QTimer(this);
    hideT->setSingleShot(true);
    connect(hideT, &QTimer::timeout, scalePerc, &DLabel::hide);

    connect(m_view, &ImageGraphicsView::scaled, this, [ = ](qreal perc) {
        label->setText(QString("%1%").arg(int(perc)));
        if (perc > 100) {

        } else if (perc == 100.0) {

        } else {

        }
    });
    connect(m_view, &ImageGraphicsView::showScaleLabel, this, [ = ]() {
        scalePerc->show();
        hideT->start(1000);
    });
}

void ViewPanel::initNavigation()
{
    m_nav = new NavigationWidget(this);
    m_nav.setBottomMargin(100);
    m_nav.setLeftMargin(10);
    m_nav.setAnchor(Qt::AnchorLeft, this, Qt::AnchorLeft);
    m_nav.setAnchor(Qt::AnchorBottom, this, Qt::AnchorBottom);

    connect(this, &ViewPanel::imageChanged, this, [ = ](const QString & path) {
        if (path.isEmpty()) m_nav->setVisible(false);
        m_nav->setImage(m_view->image());
    });

    connect(m_nav, &NavigationWidget::requestMove, [this](int x, int y) {
        m_view->centerOn(x, y);
    });
    connect(m_view, &ImageGraphicsView::transformChanged, [this]() {
        //如果stackindex不为2，全屏会出现导航窗口
        //如果是正在移动的情况，将不会出现导航栏窗口
        if (m_stack->currentIndex() != 2) {
            m_nav->setVisible((! m_nav->isAlwaysHidden() && ! m_view->isWholeImageVisible()));
            m_nav->setRectInImage(m_view->visibleImageRect());
        }
    });
    m_nav->show();
}

void ViewPanel::initRightMenu()
{
    if (!m_menu) {
        m_menu = new DMenu(this);
        updateMenuContent();
    }
    QShortcut *ctrlm = new QShortcut(QKeySequence("Ctrl+M"), this);
    ctrlm->setContext(Qt::WindowShortcut);
    connect(ctrlm, &QShortcut::activated, this, [ = ] {
        this->customContextMenuRequested(cursor().pos());
    });

    m_menu = new DMenu;
    connect(this, &ViewPanel::customContextMenuRequested, this, [ = ] {

        updateMenuContent();
        m_menu->popup(QCursor::pos());
    });
    connect(m_menu, &DMenu::triggered, this, &ViewPanel::onMenuItemClicked);
}

void ViewPanel::initExtensionPanel()
{
    if (!m_info) {
        m_info = new ImageInfoWidget("", "", this);
        m_info->hide();
    }
    m_info->setImagePath(m_bottomToolbar->getCurrentItemInfo().path);
    if (!m_extensionPanel) {
        m_extensionPanel = new ExtensionPanel(this);
        connect(m_info, &ImageInfoWidget::extensionPanelHeight, m_extensionPanel, &ExtensionPanel::updateRectWithContent);
        connect(m_view, &ImageGraphicsView::clicked, this, [ = ] {
            this->m_extensionPanel->hide();
            this->m_info->show();
        });
    }
}

void ViewPanel::updateMenuContent()
{
    if (m_menu) {
        m_menu->clear();
        qDeleteAll(this->actions());

        imageViewerSpace::ItemInfo ItemInfo = m_bottomToolbar->getCurrentItemInfo();

        QFileInfo info(ItemInfo.path);
        bool isReadable = info.isReadable();//是否可读
        bool isWritable = info.isWritable();//是否可写
        bool isRotatable = ImageEngine::instance()->isRotatable(ItemInfo.path);//是否可旋转
        imageViewerSpace::PathType pathType = ItemInfo.pathType;//路径类型
        imageViewerSpace::ImageType imageType = ItemInfo.imageType;//图片类型

        if (imageViewerSpace::ImageTypeDamaged == imageType) {
            return;
        }

        if (window()->isFullScreen()) {
            appendAction(IdExitFullScreen, QObject::tr("Exit fullscreen"), ss("Fullscreen", "F11"));
        } else {
            appendAction(IdFullScreen, QObject::tr("Fullscreen"), ss("Fullscreen", "F11"));
        }

        appendAction(IdPrint, QObject::tr("Print"), ss("Print", "Ctrl+P"));
        //ocr按钮,是否是动态图,todo
        if (imageViewerSpace::ImageTypeDynamic != imageType) {
            appendAction(IdOcr, QObject::tr("Extract text"), ss("Extract text", "Alt+O"));
        }

        //如果图片数量大于0才能有幻灯片
        appendAction(IdStartSlideShow, QObject::tr("Slide show"), ss("Slide show", "F5"));

        m_menu->addSeparator();

        appendAction(IdCopy, QObject::tr("Copy"), ss("Copy", "Ctrl+C"));

        //如果程序有可读可写的权限,才能重命名,todo
        if (isReadable && isWritable) {
            appendAction(IdRename, QObject::tr("Rename"), ss("Rename", "F2"));
        }

        //apple phone的delete没有权限,保险箱无法删除,垃圾箱也无法删除,其他需要判断可读权限,todo
        if (imageViewerSpace::PathTypeAPPLE != pathType  && imageViewerSpace::PathTypeSAFEBOX != pathType && imageViewerSpace::PathTypeRECYCLEBIN != pathType) {
            appendAction(IdMoveToTrash, QObject::tr("Delete"), ss("Throw to trash", "Delete"));
        }

        m_menu->addSeparator();

        //判断导航栏隐藏,需要添加一个当前是否有图片,todo
        if (!m_view->isWholeImageVisible() && m_nav->isAlwaysHidden()) {
            appendAction(IdShowNavigationWindow, QObject::tr("Show navigation window"),
                         ss("Show navigation window", ""));
        } else if (!m_view->isWholeImageVisible() && !m_nav->isAlwaysHidden()) {
            appendAction(IdHideNavigationWindow, QObject::tr("Hide navigation window"),
                         ss("Hide navigation window", ""));
        }
        //apple手机特殊处理，不具备旋转功能,todo
        if (imageViewerSpace::PathTypeAPPLE != pathType  && imageViewerSpace::PathTypeSAFEBOX != pathType && imageViewerSpace::PathTypeRECYCLEBIN != pathType) {
            appendAction(IdRotateClockwise, QObject::tr("Rotate clockwise"), ss("Rotate clockwise", "Ctrl+R"));
            appendAction(IdRotateCounterclockwise, QObject::tr("Rotate counterclockwise"),
                         ss("Rotate counterclockwise", "Ctrl+Shift+R"));
        }

        //需要判断图片是否支持设置壁纸,todo
        if (utils::image::imageSupportWallPaper(ItemInfo.path)) {
            appendAction(IdSetAsWallpaper, QObject::tr("Set as wallpaper"), ss("Set as wallpaper", "Ctrl+F9"));
        }
        appendAction(IdDisplayInFileManager, QObject::tr("Display in file manager"),
                     ss("Display in file manager", "Alt+D"));
        appendAction(IdImageInfo, QObject::tr("Image info"), ss("Image info", "Ctrl+I"));

    }
}

void ViewPanel::toggleFullScreen()
{
//    m_view->setFitState(false, false);
    if (window()->isFullScreen()) {
        showNormal();
        m_view->viewport()->setCursor(Qt::ArrowCursor);
    } else {
        showFullScreen();
        if (!m_menu || !m_menu->isVisible()) {
            m_view->viewport()->setCursor(Qt::BlankCursor);
        }
    }
}

void ViewPanel::showFullScreen()
{
    m_isMaximized = window()->isMaximized();
    // Full screen then hide bars because hide animation depends on height()
    //加入动画效果，掩盖左上角展开的视觉效果，以透明度0-1显示。,时间为50ms

    QPropertyAnimation *pAn = new QPropertyAnimation(window(), "windowOpacity");
    pAn->setDuration(50);
    pAn->setEasingCurve(QEasingCurve::Linear);
    pAn->setEndValue(1);
    pAn->setStartValue(0);
    pAn->start(QAbstractAnimation::DeleteWhenStopped);


    window()->showFullScreen();

}

void ViewPanel::showNormal()
{
    //加入动画效果，掩盖左上角展开的视觉效果，以透明度0-1显示。

    QPropertyAnimation *pAn = new QPropertyAnimation(window(), "windowOpacity");
    pAn->setDuration(50);
    pAn->setEasingCurve(QEasingCurve::Linear);
    pAn->setEndValue(1);
    pAn->setStartValue(0);
    pAn->start(QAbstractAnimation::DeleteWhenStopped);
    if (m_isMaximized) {
        window()->showNormal();
        window()->showMaximized();
    } else {
        window()->showNormal();
    }
}

void ViewPanel::appendAction(int id, const QString &text, const QString &shortcut)
{
    if (m_menu) {
        QAction *ac = new QAction(m_menu);
        addAction(ac);
        ac->setText(text);
        ac->setProperty("MenuID", id);
        ac->setShortcut(QKeySequence(shortcut));
        m_menu->addAction(ac);
    }
}

void ViewPanel::onMenuItemClicked(QAction *action)
{
    const int id = action->property("MenuID").toInt();
    switch (MenuItemId(id)) {
    case IdFullScreen:
    case IdExitFullScreen: {
        toggleFullScreen();
        break;
    }
    case IdStartSlideShow: {
        //todo,幻灯片
        break;
    }
    case IdPrint: {
        PrintHelper::getIntance()->showPrintDialog(QStringList(m_bottomToolbar->getCurrentItemInfo().path), this);
        break;
    }
    case IdRename: {
        //todo,重命名
        break;
    }
    case IdCopy: {
        //todo,复制
        utils::base::copyImageToClipboard(QStringList(m_bottomToolbar->getCurrentItemInfo().path));
        break;
    }
    case IdMoveToTrash: {
        //todo,删除
        if (m_bottomToolbar) {
            m_bottomToolbar->deleteImage();
        }
        break;
    }
    case IdShowNavigationWindow: {
        m_nav->setAlwaysHidden(false);
        break;
    }
    case IdHideNavigationWindow: {
        m_nav->setAlwaysHidden(true);
        break;
    }
    case IdRotateClockwise: {
        //todo旋转
        if (m_bottomToolbar) {
            m_bottomToolbar->onRotateLBtnClicked();
        }
        break;
    }
    case IdRotateCounterclockwise: {
        //todo旋转
        if (m_bottomToolbar) {
            m_bottomToolbar->onRotateRBtnClicked();
        }
        break;
    }
    case IdSetAsWallpaper: {
        //todo设置壁纸
        break;
    }
    case IdDisplayInFileManager : {
        //todo显示在文管
        utils::base::showInFileManager(m_bottomToolbar->getCurrentItemInfo().path);
        break;
    }
    case IdImageInfo: {
        //todo,文件信息
        if (!m_info && !m_extensionPanel) {
            initExtensionPanel();
        }
        m_info->show();
        m_info->setImagePath(m_bottomToolbar->getCurrentItemInfo().path);
        m_extensionPanel->setContent(m_info);
        m_extensionPanel->show();
        if (this->window()->isFullScreen() || this->window()->isMaximized()) {
            m_extensionPanel->move(this->window()->width() - m_extensionPanel->width() - 24,
                                   TOP_TOOLBAR_HEIGHT * 2);
        } else {
            m_extensionPanel->move(this->window()->pos() +
                                   QPoint(this->window()->width() - m_extensionPanel->width() - 24,
                                          TOP_TOOLBAR_HEIGHT * 2));
        }
        break;
    }
    case IdOcr: {
        //todo,ocr
        break;
    }
    }

}

void ViewPanel::resetBottomToolbarGeometry(bool visible)
{
    m_bottomToolbar->setVisible(visible);
    if (visible) {
        int width = qMin(m_bottomToolbar->getToolbarWidth(), (this->width() - RT_SPACING));
        int x = (this->width() - width) / 2;
        //窗口高度-工具栏高度-工具栏到底部距离
        int y = this->height() - BOTTOM_TOOLBAR_HEIGHT - BOTTOM_SPACING;
        m_bottomToolbar->setGeometry(x, y, width, BOTTOM_TOOLBAR_HEIGHT);
    }
}

void ViewPanel::openImg(int index, QString path)
{
    //展示图片
    m_view->setImage(path);
    m_view->resetTransform();
}

void ViewPanel::slotRotateImage(int angle)
{
    if (m_view) {
        m_view->slotRotatePixmap(angle);
    }
    //实时保存太卡，因此采用2s后延时保存的问题
    if (!m_tSaveImage) {
        m_tSaveImage = new QTimer(this);
        connect(m_tSaveImage, &QTimer::timeout, this, [ = ]() {
            m_view->slotRotatePixCurrent();
        });
    }

    m_tSaveImage->setSingleShot(true);
    m_tSaveImage->start(2000);
    m_view->autoFit();
}

void ViewPanel::resizeEvent(QResizeEvent *e)
{
    if (m_extensionPanel) {
        // 获取widget左上角坐标的全局坐标
        //lmh0826,解决bug44826
        QPoint p = this->mapToGlobal(QPoint(0, 0));
        m_extensionPanel->move(p + QPoint(this->window()->width() - m_extensionPanel->width() - 24,
                                          TOP_TOOLBAR_HEIGHT * 2));
    }
    if (this->m_topToolbar) {

        if (window()->isFullScreen()) {
            this->m_topToolbar->setVisible(false);
        } else {
            this->m_topToolbar->setVisible(true);
        }

        if (m_topToolbar->isVisible()) {
            this->m_topToolbar->resize(width(), 50);
        }
    }
    resetBottomToolbarGeometry(m_stack->currentWidget() == m_view);
    QFrame::resizeEvent(e);
}

void ViewPanel::showEvent(QShowEvent *e)
{
//    resetBottomToolbarGeometry(m_stack->currentWidget() == m_view);
    QFrame::showEvent(e);
}

void ViewPanel::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);
    qDebug() << "windows flgs ========= " << this->windowFlags() << "attributs = " << this->testAttribute(Qt::WA_Resized);
}