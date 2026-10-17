/*
 * Copyright (C) 2021 LingmoOS Team.
 *
 * Author:     rekols <revenmartin@gmail.com>
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

#ifndef DOCKSETTINGS_H
#define DOCKSETTINGS_H

#include <QObject>
#include <QSettings>

class DockSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Direction direction READ direction WRITE setDirection NOTIFY directionChanged)
    Q_PROPERTY(int iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)
    Q_PROPERTY(int edgeMargins READ edgeMargins WRITE setEdgeMargins)
    Q_PROPERTY(bool roundedWindowEnabled READ roundedWindowEnabled WRITE setRoundedWindowEnabled NOTIFY roundedWindowEnabledChanged)
    Q_PROPERTY(Style style READ style WRITE setStyle NOTIFY styleChanged)
    Q_PROPERTY(Background background READ background WRITE setBackground NOTIFY backgroundChanged)
    Q_PROPERTY(bool magnification READ magnification WRITE setMagnification NOTIFY magnificationChanged)
    Q_PROPERTY(int magnificationSize READ magnificationSize WRITE setMagnificationSize NOTIFY magnificationSizeChanged)

public:
    enum Direction {
        Left = 0,
        Bottom,
        Right
    };
    Q_ENUMS(Direction)

    enum Visibility {
        AlwaysShow = 0,
        // AutoHide,
        AlwaysHide,
        IntellHide
    };
    Q_ENUMS(Visibility)

    enum Style {
        Round = 0,
        Straight
    };
    Q_ENUMS(Style)

    // What's behind the icons: the tinted panel, see-through glass, or nothing
    enum Background {
        Solid = 0,
        Glass,
        NoBackground
    };
    Q_ENUMS(Background)

    static DockSettings *self();
    explicit DockSettings(QObject *parent = nullptr);

    int iconSize() const;
    void setIconSize(int iconSize);

    Direction direction() const;
    void setDirection(const Direction &direction);

    Visibility visibility() const;
    void setVisibility(const Visibility &visibility);

    int edgeMargins() const;
    void setEdgeMargins(int edgeMargins);

    bool roundedWindowEnabled() const;
    void setRoundedWindowEnabled(bool enabled);

    Style style() const;
    void setStyle(const Style &style);

    Background background() const;
    void setBackground(Background background);

    // Icons grow under the pointer (off by default)
    bool magnification() const;
    void setMagnification(bool enabled);

    // How big the icon under the pointer gets, in percent of its normal size
    // (dock.conf MagnificationSize, 125-200)
    static constexpr int MinMagnificationSize = 125;
    static constexpr int MaxMagnificationSize = 200;
    int magnificationSize() const;
    void setMagnificationSize(int percent);

signals:
    void iconSizeChanged();
    void directionChanged();
    void visibilityChanged();
    void roundedWindowEnabledChanged();
    void styleChanged();
    void backgroundChanged();
    void magnificationChanged();
    void magnificationSizeChanged();

private:
    int m_iconSize;
    int m_edgeMargins;
    bool m_roundedWindowEnabled;
    Direction m_direction;
    Visibility m_visibility;
    Style m_style;
    Background m_background = Solid;
    bool m_magnification = false;
    int m_magnificationSize = 150;
    QSettings *m_settings;
};

#endif // DOCKSETTINGS_H
