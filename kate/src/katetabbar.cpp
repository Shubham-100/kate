/*   This file is part of the KDE project
 *
 *   Copyright (C) 2014 Dominik Haumann <dhauumann@kde.org>
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Library General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public License
 *   along with this library; see the file COPYING.LIB.  If not, write to
 *   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *   Boston, MA 02110-1301, USA.
 */

#include "katetabbar.h"
#include "katetabbutton.h"

#include <kconfig.h>
#include <kconfiggroup.h>
#include <kiconloader.h>
#include <kstringhandler.h>
#include <KLocalizedString>

#include <QToolButton>
#include <QApplication> // QApplication::sendEvent
#include <QDebug>

/**
 * Creates a new tab bar with the given \a parent and \a name.
 *  .
 */
KateTabBar::KateTabBar(QWidget *parent)
    : QWidget(parent)
{
    m_minimumTabWidth = 150;
    m_maximumTabWidth = 350;

    m_nextID = 0;

    m_activeButton = 0L;

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}

/**
 * Destroys the tab bar.
 */
KateTabBar::~KateTabBar()
{
}

/**
 * Loads the settings from \a config from section \a group.
 * Remembered properties are:
 *  - minimum and maximum tab width
 *  - fixed tab height
 *  - button colors
 *  - much more!
 *  .
 * The original group is saved and restored at the end of this function.
 *
 * \note Call @p load() immediately after you created the tabbar, otherwise
 *       some properties might not be restored correctly (like highlighted
 *       buttons).
 */
void KateTabBar::load(KConfigBase *config, const QString &group)
{
    KConfigGroup cg(config, group);

    // tabbar properties
    setMinimumTabWidth(cg.readEntry("minimum width", m_minimumTabWidth));
    setMaximumTabWidth(cg.readEntry("maximum width", m_maximumTabWidth));

    // highlighted entries
    QStringList documents = cg.readEntry("highlighted documents", QStringList());
    QStringList colors = cg.readEntry("highlighted colors", QStringList());

    // restore highlight map
    m_highlightedTabs.clear();
    for (int i = 0; i < documents.size() && i < colors.size(); ++i) {
        m_highlightedTabs[documents[i]] = colors[i];
    }

    setHighlightMarks(highlightMarks());
}

/**
 * Saves the settings to \a config into section \a group.
 * The original group is saved and restored at the end of this function.
 * See @p load() for more information.
 */
void KateTabBar::save(KConfigBase *config, const QString &group) const
{
    KConfigGroup cg(config, group);

    // tabbar properties
    cg.writeEntry("minimum width", minimumTabWidth());
    cg.writeEntry("maximum width", maximumTabWidth());

    // highlighted entries
    cg.writeEntry("highlighted documents", m_highlightedTabs.keys());
    cg.writeEntry("highlighted colors", m_highlightedTabs.values());
}

/**
 * Set the minimum width in pixels a tab must have.
 */
void KateTabBar::setMinimumTabWidth(int min_pixel)
{
    if (m_minimumTabWidth == min_pixel) {
        return;
    }

    m_minimumTabWidth = min_pixel;
    updateButtonPositions();
}

/**
 * Set the maximum width in pixels a tab may have.
 */
void KateTabBar::setMaximumTabWidth(int max_pixel)
{
    if (m_maximumTabWidth == max_pixel) {
        return;
    }

    m_maximumTabWidth = max_pixel;
    updateButtonPositions();
}

/**
 * Get the minimum width in pixels a tab can have.
 */
int KateTabBar::minimumTabWidth() const
{
    return m_minimumTabWidth;
}

/**
 * Get the maximum width in pixels a tab can have.
 */
int KateTabBar::maximumTabWidth() const
{
    return m_maximumTabWidth;
}

/**
 * Adds a new tab with \a text. Returns the new tab's id.
 */
int KateTabBar::addTab(const QString &text)
{
    return insertTab(m_tabButtons.size(), text);
}

int KateTabBar::insertTab(int position, const QString & text)
{
    Q_ASSERT(position <= m_tabButtons.size());

    // -1 is append
    if (position < 0) {
        position = m_tabButtons.size();
    }

    KateTabButton *tabButton = new KateTabButton(text, this);
    if (m_highlightedTabs.contains(text)) {
        tabButton->setHighlightColor(QColor(m_highlightedTabs[text]));
    }

    m_tabButtons.insert(position, tabButton);
    m_IDToTabButton[m_nextID] = tabButton;
    connect(tabButton, SIGNAL(activated(KateTabButton*)),
            this, SLOT(tabButtonActivated(KateTabButton*)));
    connect(tabButton, SIGNAL(highlightChanged(KateTabButton*)),
            this, SLOT(tabButtonHighlightChanged(KateTabButton*)));
    connect(tabButton, SIGNAL(closeRequest(KateTabButton*)),
            this, SLOT(tabButtonCloseRequest(KateTabButton*)));
    connect(tabButton, SIGNAL(closeOtherTabsRequest(KateTabButton*)),
            this, SLOT(tabButtonCloseOtherRequest(KateTabButton*)));
    connect(tabButton, SIGNAL(closeAllTabsRequest()),
            this, SLOT(tabButtonCloseAllRequest()));

    updateButtonPositions();

    return m_nextID++;
}

/**
 * Get the ID of the tab bar's activated tab. Returns -1 if no tab is activated.
 */
int KateTabBar::currentTab() const
{
    return m_IDToTabButton.key(m_activeButton, -1);
}

/**
 * Activate the tab with ID \a index. No signal is emitted.
 */
void KateTabBar::setCurrentTab(int index)
{
    if (!m_IDToTabButton.contains(index)) {
        return;
    }

    KateTabButton *tabButton = m_IDToTabButton[index];
    if (m_activeButton == tabButton) {
        return;
    }

    if (m_activeButton) {
        m_activeButton->setActivated(false);
    }

    m_activeButton = tabButton;
    m_activeButton->setActivated(true);
}

/**
 * Removes the tab with ID \a id.
 * @return the position where the tab was
 */
int KateTabBar::removeTab(int id)
{
    if (!m_IDToTabButton.contains(id)) {
        Q_ASSERT(false);
        return -1;
    }

    KateTabButton *tabButton = m_IDToTabButton[id];

    if (tabButton == m_activeButton) {
        m_activeButton = 0L;
    }

    const int position = m_tabButtons.indexOf(tabButton);

    m_IDToTabButton.remove(id);
    m_tabButtons.removeAt(position);
    // delete the button with deleteLater() because the button itself might
    // have send a close-request. So the app-execution is still in the
    // button, a delete tabButton; would lead to a crash.
    tabButton->hide();
    tabButton->deleteLater();

    updateButtonPositions();

    return position;
}

/**
 * Returns whether a tab with ID \a index exists.
 */
bool KateTabBar::containsTab(int index) const
{
    return m_IDToTabButton.contains(index);
}

/**
 * Sets the text of the tab with ID \a index to \a text.
 * \see tabText()
 */
void KateTabBar::setTabText(int index, const QString &text)
{
    if (!m_IDToTabButton.contains(index)) {
        return;
    }

    // change highlight key, if entry exists
    if (m_highlightedTabs.contains(m_IDToTabButton[index]->text())) {
        QString value = m_highlightedTabs[m_IDToTabButton[index]->text()];
        m_highlightedTabs.remove(m_IDToTabButton[index]->text());
        m_highlightedTabs[text] = value;

        // do not emit highlightMarksChanged(), because every tabbar gets this
        // change (usually)
        // emit highlightMarksChanged( this );
    }

    m_IDToTabButton[index]->setText(text);
}

/**
 * Returns the text of the tab with ID \a index. If the button id does not
 * exist \a QString() is returned.
 * \see setTabText()
 */
QString KateTabBar::tabText(int index) const
{
    if (m_IDToTabButton.contains(index)) {
        return m_IDToTabButton[index]->text();
    }

    return QString();
}

/**
 * Set the button @p index's tool tip to @p tip.
 */
void KateTabBar::setTabToolTip(int index, const QString &tip)
{
    if (!m_IDToTabButton.contains(index)) {
        return;
    }

    m_IDToTabButton[index]->setToolTip(tip);
}

/**
 * Get the button @p index's url. Result is QStrint() if not available.
 */
QString KateTabBar::tabToolTip(int index) const
{
    if (m_IDToTabButton.contains(index)) {
        return m_IDToTabButton[index]->toolTip();
    }

    return QString();
}

/**
 * Sets the icon of the tab with ID \a index to \a icon.
 * \see tabIcon()
 */
void KateTabBar::setTabIcon(int index, const QIcon &icon)
{
    if (m_IDToTabButton.contains(index)) {
        m_IDToTabButton[index]->setIcon(icon);
    }
}

/**
 * Returns the icon of the tab with ID \a index. If the button id does not
 * exist \a QIcon() is returned.
 * \see setTabIcon()
 */
QIcon KateTabBar::tabIcon(int index) const
{
    if (m_IDToTabButton.contains(index)) {
        return m_IDToTabButton[index]->icon();
    }

    return QIcon();
}

/**
 * Returns the number of tabs in the tab bar.
 */
int KateTabBar::count() const
{
    return m_tabButtons.count();
}

void KateTabBar::setTabModified(int index, bool modified)
{
    if (m_IDToTabButton.contains(index)) {
        m_IDToTabButton[index]->setModified(modified);
    }
}

bool KateTabBar::isTabModified(int index) const
{
    if (m_IDToTabButton.contains(index)) {
        return m_IDToTabButton[index]->isModified();
    }

    return false;
}

void KateTabBar::removeHighlightMarks()
{
    KateTabButton *tabButton;
    foreach(tabButton, m_tabButtons) {
        if (tabButton->highlightColor().isValid()) {
            tabButton->setHighlightColor(QColor());
        }
    }

    m_highlightedTabs.clear();
    emit highlightMarksChanged(this);
}

void KateTabBar::setHighlightMarks(const QMap<QString, QString> &marks)
{
    m_highlightedTabs = marks;

    KateTabButton *tabButton;
    foreach(tabButton, m_tabButtons) {
        if (marks.contains(tabButton->text())) {
            if (tabButton->highlightColor().name() != marks[tabButton->text()]) {
                tabButton->setHighlightColor(QColor(marks[tabButton->text()]));
            }
        } else if (tabButton->highlightColor().isValid()) {
            tabButton->setHighlightColor(QColor());
        }
    }
}

QMap<QString, QString> KateTabBar::highlightMarks() const
{
    return m_highlightedTabs;
}

/**
 * Active button changed. Emit signal \p currentChanged() with the button's ID.
 */
void KateTabBar::tabButtonActivated(KateTabButton *tabButton)
{
    if (tabButton == m_activeButton) {
        return;
    }

    if (m_activeButton) {
        m_activeButton->setActivated(false);
    }

    m_activeButton = tabButton;
    m_activeButton->setActivated(true);

    const int id = m_IDToTabButton.key(m_activeButton, -1);
    Q_ASSERT(id >= 0);
    emit currentChanged(id);
}

/**
 * The \e tabButton's highlight color changed, so update the list of documents
 * and colors.
 */
void KateTabBar::tabButtonHighlightChanged(KateTabButton *tabButton)
{
    if (tabButton->highlightColor().isValid()) {
        m_highlightedTabs[tabButton->text()] = tabButton->highlightColor().name();
        emit highlightMarksChanged(this);
    } else if (m_highlightedTabs.contains(tabButton->text())) {
        // invalid color, so remove the item
        m_highlightedTabs.remove(tabButton->text());
        emit highlightMarksChanged(this);
    }
}

/**
 * If the user wants to close a tab with the context menu, it sends a close
 * request. Throw the close request by emitting the signal @p closeRequest().
 */
void KateTabBar::tabButtonCloseRequest(KateTabButton *tabButton)
{
    const int id = m_IDToTabButton.key(tabButton, -1);
    Q_ASSERT(id >= 0);
    emit closeTabRequested(id);
}

/**
 * If the user wants to close all tabs except the current one using the context
 * menu, it sends multiple close requests.
 * Throw the close requests by emitting the signal @p closeRequest().
 */
void KateTabBar::tabButtonCloseOtherRequest(KateTabButton *tabButton)
{
    const int id = m_IDToTabButton.key(tabButton, -1);
    Q_ASSERT(id >= 0);
    emit closeOtherTabsRequested(id);
}

/**
 * If the user wants to close all the tabs using the context menu, it sends
 * multiple close requests.
 * Throw the close requests by emitting the signal @p closeRequest().
 */
void KateTabBar::tabButtonCloseAllRequest()
{
    emit closeAllTabsRequested();
}

/**
 * Recalculate geometry for all children.
 */
void KateTabBar::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)

    // fix button positions
    updateButtonPositions();

    const int tabDiff = maxTabCount() - m_tabButtons.size();
    if (tabDiff > 0) {
        emit moreTabsRequested(tabDiff);
    } else if (tabDiff < 0) {
        emit lessTabsRequested(-tabDiff);
    }
}

void KateTabBar::updateButtonPositions()
{
    // if there are no tabs there is nothing to do
    if (m_tabButtons.count() == 0) {
        return;
    }

    const int barWidth = width();
    const int maxCount = maxTabCount();

    // how many tabs do we show?
    const int visibleTabCount = qMin(count(), maxCount);

    // new tab width of each tab
    const qreal tabWidth = qMin(static_cast<qreal>(barWidth) / visibleTabCount,
                                static_cast<qreal>(m_maximumTabWidth));

    // now set the sizes
    const int maxi = m_tabButtons.size();
    const int w = ceil(tabWidth);
    const int h = height();
    for (int i = 0; i < maxi; ++i) {
        KateTabButton *tabButton = m_tabButtons.value(i);
        if (i >= maxCount) {
            tabButton->hide();
        } else {

            tabButton->setGeometry(ceil(i * tabWidth), 0, w, h);
            tabButton->show();
        }
    }
}

/**
 * Return the maximum amount of tabs that fit into the tab bar given
 * the minimumTabWidth().
 */
int KateTabBar::maxTabCount() const
{
    return qMax(1, width() / minimumTabWidth());
}
