#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "winmm.lib")

const int SAMPLE_RATE = 44100;
const double PI = 3.14159265358979323846;

// ------------------------------------------------------------
// COLORS
// ------------------------------------------------------------

const COLORREF BACKGROUND = RGB(25, 25, 30);
const COLORREF PANEL = RGB(35, 35, 42);
const COLORREF BUTTON = RGB(55, 55, 65);
const COLORREF BUTTON_HOVER = RGB(70, 70, 85);
const COLORREF ACCENT = RGB(80, 150, 255);
const COLORREF TEXT = RGB(240, 240, 245);
const COLORREF COLUMN = RGB(32, 32, 38);
const COLORREF COLUMN_BORDER = RGB(55, 55, 65);
// ------------------------------------------------------------
// GLOBAL STATE
// ------------------------------------------------------------

std::atomic<bool> playing(false);
std::atomic<bool> looping(false);
std::atomic<bool> stopRequested(false);

HWND playButton;
HWND loopButton;

std::mt19937 randomGenerator(12345);

// ------------------------------------------------------------
// SONG DATA
// ------------------------------------------------------------

struct NoteEvent
{
    double start;
    double duration;
    double frequency;
};

struct DrumEvent
{
    double start;
    std::string type;
};

// ------------------------------------------------------------
// NOISE
// ------------------------------------------------------------

double noise()
{
    static std::uniform_real_distribution<double> distribution(-1.0, 1.0);

    return distribution(randomGenerator);
}

// ------------------------------------------------------------
// NOTE FREQUENCIES
// ------------------------------------------------------------

double noteFrequency(const std::string& note)
{
    if (note == "C3") return 130.81;
    if (note == "D3") return 146.83;
    if (note == "E3") return 164.81;
    if (note == "F3") return 174.61;
    if (note == "G3") return 196.00;
    if (note == "A3") return 220.00;
    if (note == "B3") return 246.94;

    if (note == "C4") return 261.63;
    if (note == "C#4") return 277.18;
    if (note == "D4") return 293.66;
    if (note == "D#4") return 311.13;
    if (note == "E4") return 329.63;
    if (note == "F4") return 349.23;
    if (note == "F#4") return 369.99;
    if (note == "G4") return 392.00;
    if (note == "G#4") return 415.30;
    if (note == "A4") return 440.00;
    if (note == "A#4") return 466.16;
    if (note == "B4") return 493.88;

    if (note == "C5") return 523.25;
    if (note == "C#5") return 554.37;
    if (note == "D5") return 587.33;
    if (note == "D#5") return 622.25;
    if (note == "E5") return 659.25;
    if (note == "F5") return 698.46;
    if (note == "F#5") return 739.99;
    if (note == "G5") return 783.99;
    if (note == "G#5") return 830.61;
    if (note == "A5") return 880.00;
    if (note == "A#5") return 932.33;
    if (note == "B5") return 987.77;

    return 0.0;
}

// ------------------------------------------------------------
// DRUMS
// ------------------------------------------------------------

double kick(double t)
{
    if (t < 0 || t >= 0.7)
        return 0.0;

    double frequency =
        150.0 * std::exp(-12.0 * t) + 45.0;

    double envelope =
        std::exp(-7.0 * t);

    return std::sin(2.0 * PI * frequency * t)
           * envelope;
}

double snare(double t)
{
    if (t < 0 || t >= 0.35)
        return 0.0;

    double envelope =
        std::exp(-15.0 * t);

    double noisePart =
        noise() * envelope;

    double body =
        std::sin(2.0 * PI * 180.0 * t)
        * std::exp(-20.0 * t);

    return noisePart * 0.8 + body * 0.2;
}

double closedHiHat(double t)
{
    if (t < 0 || t >= 0.12)
        return 0.0;

    return noise()
           * std::exp(-45.0 * t)
           * 0.7;
}

double openHiHat(double t)
{
    if (t < 0 || t >= 0.8)
        return 0.0;

    return noise()
           * std::exp(-5.0 * t)
           * 0.6;
}

double clap(double t)
{
    if (t < 0 || t >= 0.3)
        return 0.0;

    double burst1 =
        std::exp(-80.0 * std::abs(t));

    double burst2 =
        std::exp(-60.0 * std::abs(t - 0.025));

    double burst3 =
        std::exp(-50.0 * std::abs(t - 0.050));

    double envelope =
        std::exp(-14.0 * t);

    return noise()
           * (burst1 + burst2 + burst3)
           * envelope;
}

double tom(double t, double frequency)
{
    if (t < 0 || t >= 0.6)
        return 0.0;

    double pitch =
        frequency * std::exp(-3.0 * t)
        + frequency * 0.4;

    double envelope =
        std::exp(-7.0 * t);

    return std::sin(2.0 * PI * pitch * t)
           * envelope;
}

double crash(double t)
{
    if (t < 0 || t >= 2.0)
        return 0.0;

    return noise()
           * std::exp(-2.5 * t)
           * 0.65;
}

double ride(double t)
{
    if (t < 0 || t >= 1.5)
        return 0.0;

    double envelope =
        std::exp(-2.0 * t);

    double metallic =
        noise();

    double tone =
        std::sin(2.0 * PI * 3500.0 * t);

    return (metallic * 0.5 + tone * 0.5)
           * envelope
           * 0.4;
}

double rimshot(double t)
{
    if (t < 0 || t >= 0.15)
        return 0.0;

    double envelope =
        std::exp(-40.0 * t);

    return (
        std::sin(2.0 * PI * 1200.0 * t)
        * envelope
        * 0.8
    )
    +
    (
        noise()
        * envelope
        * 0.3
    );
}

double cowbell(double t)
{
    if (t < 0 || t >= 0.3)
        return 0.0;

    double envelope =
        std::exp(-15.0 * t);

    double tone1 =
        std::sin(2.0 * PI * 540.0 * t);

    double tone2 =
        std::sin(2.0 * PI * 800.0 * t);

    return (tone1 + tone2)
           * envelope
           * 0.4;
}

double shaker(double t)
{
    if (t < 0 || t >= 0.25)
        return 0.0;

    return noise()
           * std::exp(-18.0 * t)
           * 0.5;
}

double tambourine(double t)
{
    if (t < 0 || t >= 0.7)
        return 0.0;

    double envelope =
        std::exp(-6.0 * t);

    double metal =
        noise();

    double ring =
        std::sin(2.0 * PI * 4000.0 * t);

    return (metal * 0.7 + ring * 0.3)
           * envelope
           * 0.5;
}

// ------------------------------------------------------------
// DRUM SELECTOR
// ------------------------------------------------------------

double makeDrum(
    const std::string& type,
    double t)
{
    if (type == "KICK" || type == "BASS_DRUM")
        return kick(t);

    if (type == "SNARE")
        return snare(t);

    if (type == "HIHAT")
        return closedHiHat(t);

    if (type == "OPEN_HIHAT")
        return openHiHat(t);

    if (type == "CLAP")
        return clap(t);

    if (type == "LOW_TOM")
        return tom(t, 110.0);

    if (type == "MID_TOM")
        return tom(t, 180.0);

    if (type == "HIGH_TOM")
        return tom(t, 280.0);

    if (type == "CRASH")
        return crash(t);

    if (type == "RIDE")
        return ride(t);

    if (type == "RIMSHOT")
        return rimshot(t);

    if (type == "COWBELL")
        return cowbell(t);

    if (type == "SHAKER")
        return shaker(t);

    if (type == "TAMBOURINE")
        return tambourine(t);

    return 0.0;
}

// ------------------------------------------------------------
// GENERATE AND PLAY SONG
// ------------------------------------------------------------

void PlaySong()
{
    if (playing)
        return;

    playing = true;
    stopRequested = false;

    while (true)
    {
        if (stopRequested)
            break;

        // ----------------------------------------------------
        // READ SONG
        // ----------------------------------------------------

        std::ifstream songFile("song.txt");

        if (!songFile)
        {
            MessageBox(
                nullptr,
                "Could not open song.txt",
                "Error",
                MB_OK | MB_ICONERROR
            );

            break;
        }

        std::vector<NoteEvent> notes;
        std::vector<DrumEvent> drums;

        // Default tempo
        // Default settings
        double tempo = 120.0;
        double loopLengthBeats = 4.0;

        std::string command;

        while (songFile >> command)
    {
        if (command == "TEMPO")
    {
        songFile >> tempo;

        if (tempo <= 0)
            tempo = 120.0;
    }

    else if (command == "LENGTH")
    {
        songFile >> loopLengthBeats;

        if (loopLengthBeats <= 0)
            loopLengthBeats = 4.0;
    }

        else if (command == "NOTE")
    {
        std::string noteName;
        double startBeat;
        double durationBeats;

        songFile >>
            noteName >>
            startBeat >>
            durationBeats;

        double frequency =
            noteFrequency(noteName);

        if (frequency > 0.0)
        {
            double secondsPerBeat =
                60.0 / tempo;

            double start =
                startBeat *
                secondsPerBeat;

            double duration =
                durationBeats *
                secondsPerBeat;

            notes.push_back({
                start,
                duration,
                frequency
            });
        }
    }

    else if (command == "DRUM")
    {
        std::string drumType;
        double startBeat;

        songFile >>
            drumType >>
            startBeat;

        double secondsPerBeat =
            60.0 / tempo;

        double start =
            startBeat *
            secondsPerBeat;

        drums.push_back({
            start,
            drumType
        });
    }
}

songFile.close();

// ----------------------------------------------------
// EXACT LOOP LENGTH
// ----------------------------------------------------

double secondsPerBeat =
    60.0 / tempo;

double loopDuration =
    loopLengthBeats *
    secondsPerBeat;

// The audio buffer is exactly the loop length.
// Drum tails do NOT extend the loop.
int totalSamples =
    static_cast<int>(
        loopDuration *
        SAMPLE_RATE
    );

if (totalSamples <= 0)
    break;

        // ----------------------------------------------------
        // AUDIO BUFFER
        // ----------------------------------------------------

        std::vector<double> audio(
            totalSamples,
            0.0
        );

        // ----------------------------------------------------
        // MELODY
        // ----------------------------------------------------

        for (const NoteEvent& note : notes)
        {
            int startSample =
                static_cast<int>(
                    note.start *
                    SAMPLE_RATE
                );

            int noteSamples =
                static_cast<int>(
                    note.duration *
                    SAMPLE_RATE
                );

            for (int i = 0;
                 i < noteSamples;
                 i++)
            {
                int index =
                    startSample + i;

                if (index >= totalSamples)
                    break;

                double time =
                    static_cast<double>(i)
                    / SAMPLE_RATE;

                double envelope = 1.0;

                if (time < 0.01)
                {
                    envelope =
                        time / 0.01;
                }

                double remaining =
                    note.duration - time;

                if (remaining < 0.05)
                {
                    envelope =
                        std::min(
                            envelope,
                            remaining / 0.05
                        );
                }

                double wave =
                    std::sin(
                        2.0 *
                        PI *
                        note.frequency *
                        time
                    );

                audio[index] +=
                    wave *
                    envelope *
                    0.25;
            }
        }

        // ----------------------------------------------------
        // DRUMS
        // ----------------------------------------------------

        for (const DrumEvent& drum : drums)
        {
            int startSample =
                static_cast<int>(
                    drum.start *
                    SAMPLE_RATE
                );

            int drumSamples =
                static_cast<int>(
                    2.0 *
                    SAMPLE_RATE
                );

            for (int i = 0;
                 i < drumSamples;
                 i++)
            {
                int index =
                    startSample + i;

                if (index >= totalSamples)
                    break;

                double time =
                    static_cast<double>(i)
                    / SAMPLE_RATE;

                audio[index] +=
                    makeDrum(
                        drum.type,
                        time
                    ) * 0.5;
            }
        }

        // ----------------------------------------------------
        // CONVERT TO 16-BIT
        // ----------------------------------------------------

        std::vector<short> samples(
            totalSamples
        );

        for (int i = 0;
             i < totalSamples;
             i++)
        {
            double value =
                std::max(
                    -1.0,
                    std::min(
                        1.0,
                        audio[i]
                    )
                );

            samples[i] =
                static_cast<short>(
                    value * 32767
                );
        }

        // ----------------------------------------------------
        // AUDIO FORMAT
        // ----------------------------------------------------

        WAVEFORMATEX format = {};

        format.wFormatTag =
            WAVE_FORMAT_PCM;

        format.nChannels = 1;

        format.nSamplesPerSec =
            SAMPLE_RATE;

        format.wBitsPerSample = 16;

        format.nBlockAlign =
            format.nChannels *
            format.wBitsPerSample /
            8;

        format.nAvgBytesPerSec =
            format.nSamplesPerSec *
            format.nBlockAlign;

        // ----------------------------------------------------
        // OPEN AUDIO
        // ----------------------------------------------------

        HWAVEOUT audioDevice;

        MMRESULT result =
            waveOutOpen(
                &audioDevice,
                WAVE_MAPPER,
                &format,
                0,
                0,
                CALLBACK_NULL
            );

        if (result != MMSYSERR_NOERROR)
        {
            MessageBox(
                nullptr,
                "Could not open audio device.",
                "Audio Error",
                MB_OK | MB_ICONERROR
            );

            break;
        }

        WAVEHDR header = {};

        header.lpData =
            reinterpret_cast<LPSTR>(
                samples.data()
            );

        header.dwBufferLength =
            static_cast<DWORD>(
                samples.size() *
                sizeof(short)
            );

        waveOutPrepareHeader(
            audioDevice,
            &header,
            sizeof(header)
        );

        waveOutWrite(
            audioDevice,
            &header,
            sizeof(header)
        );

        // ----------------------------------------------------
        // WAIT WHILE PLAYING
        // ----------------------------------------------------

        double duration =
            static_cast<double>(
                samples.size()
            ) / SAMPLE_RATE;

        int milliseconds =
            static_cast<int>(
                duration * 1000
            );

        int elapsed = 0;

        while (
            elapsed < milliseconds &&
            !stopRequested
        )
        {
            Sleep(50);
            elapsed += 50;
        }

        // ----------------------------------------------------
        // STOP AUDIO
        // ----------------------------------------------------

        waveOutReset(audioDevice);

        waveOutUnprepareHeader(
            audioDevice,
            &header,
            sizeof(header)
        );

        waveOutClose(audioDevice);

        // ----------------------------------------------------
        // LOOP?
        // ----------------------------------------------------

        if (!looping || stopRequested)
            break;
    }

    playing = false;

    PostMessage(
        GetActiveWindow(),
        WM_USER + 1,
        0,
        0
    );
}

// ------------------------------------------------------------
// WINDOW PROCEDURE
// ------------------------------------------------------------

LRESULT CALLBACK WindowProcedure(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
        case WM_COMMAND:
        {
            int button =
                LOWORD(wParam);

            // PLAY / STOP
            if (button == 1)
            {
                if (!playing)
                {
                    SetWindowText(
                        playButton,
                        "STOP"
                    );

                    std::thread(
                        PlaySong
                    ).detach();
                }
                else
                {
                    stopRequested = true;

                    SetWindowText(
                        playButton,
                        "PLAY"
                    );
                }
            }

            // LOOP
            if (button == 2)
            {
                looping =
                    !looping;

                if (looping)
                {
                    SetWindowText(
                        loopButton,
                        "LOOP: ON"
                    );
                }
                else
                {
                    SetWindowText(
                        loopButton,
                        "LOOP: OFF"
                    );
                }
            }

            break;
        }

        case WM_USER + 1:
        {
            SetWindowText(
                playButton,
                "PLAY"
            );

            break;
        }

        case WM_PAINT:
{
    PAINTSTRUCT ps;

    HDC dc = BeginPaint(
        window,
        &ps
    );

    RECT rect;

    GetClientRect(
        window,
        &rect
    );

    // ----------------------------------------------------
    // BACKGROUND
    // ----------------------------------------------------

    HBRUSH backgroundBrush =
        CreateSolidBrush(BACKGROUND);

    FillRect(
        dc,
        &rect,
        backgroundBrush
    );

    DeleteObject(backgroundBrush);

    // ----------------------------------------------------
    // FOUR COLUMNS
    // ----------------------------------------------------

    int columnWidth =
        rect.right / 4;

    for (int i = 0; i < 4; i++)
    {
        RECT columnRect =
        {
            i * columnWidth + 5,
            5,
            (i + 1) * columnWidth - 5,
            rect.bottom - 5
        };

        HBRUSH columnBrush =
            CreateSolidBrush(COLUMN);

        FillRect(
            dc,
            &columnRect,
            columnBrush
        );

        DeleteObject(columnBrush);

        // Column border
        HPEN borderPen =
            CreatePen(
                PS_SOLID,
                1,
                COLUMN_BORDER
            );

        HPEN oldPen =
            (HPEN)SelectObject(
                dc,
                borderPen
            );

        HBRUSH oldBrush =
            (HBRUSH)SelectObject(
                dc,
                GetStockObject(NULL_BRUSH)
            );

        Rectangle(
            dc,
            columnRect.left,
            columnRect.top,
            columnRect.right,
            columnRect.bottom
        );

        SelectObject(
            dc,
            oldBrush
        );

        SelectObject(
            dc,
            oldPen
        );

        DeleteObject(borderPen);
    }

    // ----------------------------------------------------
    // COLUMN TITLES
    // ----------------------------------------------------

    SetTextColor(
        dc,
        TEXT
    );

    SetBkMode(
        dc,
        TRANSPARENT
    );

    HFONT titleFont =
        CreateFont(
            22,
            0,
            0,
            0,
            FW_BOLD,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY,
            DEFAULT_PITCH,
            "Segoe UI"
        );

    HFONT oldFont =
        (HFONT)SelectObject(
            dc,
            titleFont
        );

    const char* titles[] =
{
    "I",
    "II",
    "III",
    "IV"
};

    for (int i = 0; i < 4; i++)
    {
        RECT titleRect =
        {
            i * columnWidth,
            20,
            (i + 1) * columnWidth,
            55
        };

        DrawText(
            dc,
            titles[i],
            -1,
            &titleRect,
            DT_CENTER |
            DT_SINGLELINE
        );
    }

    SelectObject(
        dc,
        oldFont
    );

    DeleteObject(titleFont);

    EndPaint(
        window,
        &ps
    );

    break;
}

        case WM_DESTROY:
        {
            stopRequested = true;

            PostQuitMessage(0);

            break;
        }

        default:
        {
            return DefWindowProc(
                window,
                message,
                wParam,
                lParam
            );
        }
    }

    return 0;
}

// ------------------------------------------------------------
// MAIN
// ------------------------------------------------------------

int WINAPI WinMain(
    HINSTANCE instance,
    HINSTANCE,
    LPSTR,
    int)
{
    const char CLASS_NAME[] =
        "CppSongMaker";

    WNDCLASS windowClass = {};

    windowClass.lpfnWndProc =
        WindowProcedure;

    windowClass.hInstance =
        instance;

    windowClass.lpszClassName =
        CLASS_NAME;

    windowClass.hbrBackground =
        CreateSolidBrush(
            BACKGROUND
        );

    RegisterClass(
        &windowClass
    );

    HWND window =
        CreateWindowEx(
            0,
            CLASS_NAME,
            "C++ Song Maker",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            900,
            600,
            nullptr,
            nullptr,
            instance,
            nullptr
        );

    if (!window)
        return 0;

    // --------------------------------------------------------
    // PLAY BUTTON
    // --------------------------------------------------------

    playButton =
    CreateWindow(
        "BUTTON",
        "PLAY",
        WS_VISIBLE |
        WS_CHILD |
        BS_PUSHBUTTON,
        25,
        70,
        175,
        60,
        window,
        (HMENU)1,
        instance,
        nullptr
    );

    // --------------------------------------------------------
    // LOOP BUTTON
    // --------------------------------------------------------

    loopButton =
    CreateWindow(
        "BUTTON",
        "LOOP: OFF",
        WS_VISIBLE |
        WS_CHILD |
        BS_PUSHBUTTON,
        25,
        145,
        175,
        50,
        window,
        (HMENU)2,
        instance,
        nullptr
    );

    ShowWindow(
        window,
        SW_SHOW
    );

    UpdateWindow(
        window
    );

    // --------------------------------------------------------
    // MESSAGE LOOP
    // --------------------------------------------------------

    MSG message = {};

    while (
        GetMessage(
            &message,
            nullptr,
            0,
            0
        )
    )
    {
        TranslateMessage(
            &message
        );

        DispatchMessage(
            &message
        );
    }

    return 0;
}
