/**
  * @file
  * @brief This file is part of the JUCE examples.
  * 
  * Copyright (c) 2022 - Raw Material Software Limited
  * The code included in this file is provided under the terms of the ISC license
  * http://www.isc.org/downloads/software-support-policy/isc-license. Permission
  * To use, copy, modify, and/or distribute this software for any purpose with or
  * without fee is hereby granted provided that the above copyright notice and
  * this permission notice appear in all copies.
  * 
  * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
  * WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
  * PURPOSE, ARE DISCLAIMED.
  * 
  * @brief Class to convert between time in a playback region and pixels on screen.
  * @author JUCE, hugo flores garcia, aldo aguilar
  */

#pragma once

#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_audio_formats/juce_audio_formats.h"

using namespace juce;

class TimeToViewScaling
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;

        virtual void zoomLevelChanged (double newPixelPerSecond) = 0;
    };

    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    TimeToViewScaling() = default;

    void zoom (double factor)
    {
        zoomLevelPixelPerSecond = jlimit (minimumZoom, minimumZoom * 32, zoomLevelPixelPerSecond * factor);
        setZoomLevel (zoomLevelPixelPerSecond);
    }

    void setZoomLevel (double pixelPerSecond)
    {
        zoomLevelPixelPerSecond = pixelPerSecond;
        listeners.call ([this] (Listener& l) { l.zoomLevelChanged (zoomLevelPixelPerSecond); });
    }

    int getXForTime (double time) const
    {
        return roundToInt (time * zoomLevelPixelPerSecond);
    }

    double getTimeForX (int x) const
    {
        return x / zoomLevelPixelPerSecond;
    }

private:
    static constexpr auto minimumZoom = 10.0;

    double zoomLevelPixelPerSecond = minimumZoom * 4;
    ListenerList<Listener> listeners;
};