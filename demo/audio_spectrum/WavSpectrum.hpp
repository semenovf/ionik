////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023 Vladislav Trifochkin
//
// This file is part of `ionik-lib`.
//
// Changelog:
//      2023.10.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/ionik/audio/wav_explorer.hpp"
#include <QObject>

class WavSpectrum: public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool good MEMBER _good NOTIFY spectrumChanged)
    Q_PROPERTY(bool stereo READ isStereo NOTIFY spectrumChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY spectrumChanged)

private:
    bool _good {false};
    ionik::audio::wav_spectrum _wavSpectrum;

public:
    Q_SIGNAL void spectrumChanged ();

    Q_INVOKABLE float leftAt (int index) const;
    Q_INVOKABLE float rightAt (int index) const;
    Q_INVOKABLE float sampleAt (int index) const;
    Q_INVOKABLE float maxSampleValue () const;

private:
    bool isStereo () const noexcept;
    int frameCount () const noexcept;

public:
    WavSpectrum (QObject * parent = nullptr);

    void setSpectrum (ionik::audio::wav_spectrum && wavSpectrum);
    void setSpectrum (ionik::audio::wav_spectrum const & wavSpectrum);
    void setFailure ();
};