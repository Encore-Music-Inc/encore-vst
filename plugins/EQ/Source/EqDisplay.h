#pragma once

#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "dsp/ResonanceFinder.h"

// The big frequency display: live spectrum, the EQ curve, draggable band
// nodes, and (Smart view) smart bands, carve activity and resonance markers.
class EqDisplay : public juce::Component
{
public:
    using Resonance = vs::Resonance;

    static juce::Colour bandColour (int i)
    {
        static const juce::uint32 c[] { 0xffe8a33d, 0xff5aa7e0, 0xff5cc98a, 0xffd36be0, 0xffe0605a, 0xff4fd1c5, 0xfff2d16b, 0xffa0a7ff };
        return juce::Colour (c[i % 8]);
    }

    explicit EqDisplay (EqProcessor& p) : proc (p)
    {
        preHist.assign (histSize, 0.0f);
        postHist.assign (histSize, 0.0f);
        fftData.assign ((size_t) fftSize * 2, 0.0f);
        window.resize ((size_t) fftSize);
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (fftSize - 1));
        postDb.assign ((size_t) fftSize / 2 + 1, -120.0f);
        prePow.assign ((size_t) fftSize / 2 + 1, 0.0f);
        tmp.resize (1 << 15);
    }

    std::function<void (int)> onSelectBand, onSelectSmart;
    std::function<void (const Resonance&)> onResonanceClicked;
    std::vector<Resonance> resonances;
    bool smartView = false, findResonances = true;
    int selectedBand = 1, selectedSmart = 0;

    // Called from the editor's timer (~30 Hz)
    void tick()
    {
        pullTap (proc.preTap, preHist, preWrite);
        pullTap (proc.postTap, postHist, postWrite);
        fs = proc.getRate();

        spectrum (postHist, postWrite);
        for (size_t b = 0; b < postDb.size(); ++b)
        {
            const float db = 20.0f * std::log10 (fftData[b] / (float) (fftSize / 4) + 1e-9f);
            postDb[b] += (db > postDb[b] ? 0.5f : 0.15f) * (db - postDb[b]);
        }

        spectrum (preHist, preWrite);
        for (size_t b = 0; b < prePow.size(); ++b)
        {
            const float m = fftData[b] / (float) (fftSize / 4);
            prePow[b] += 0.04f * (m * m - prePow[b]); // ~1 s average: steady resonances stand out, transients don't
        }

        if (++frame % 3 == 0 && findResonances)
            detectResonances();

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        namespace theme = vs::theme;
        refreshCaches();
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::panel);
        g.fillRoundedRectangle (r, 8.0f);
        plot = r.reduced (10.0f, 8.0f).withTrimmedBottom (14.0f);

        // grid
        g.setFont (juce::FontOptions (10.0f));
        for (float f : { 30.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
        {
            const float x = xFor (f);
            g.setColour (theme::edge);
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (theme::dim);
            const juce::String t = f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f);
            g.drawText (t, juce::Rectangle<float> (x - 20.0f, plot.getBottom() + 1.0f, 40.0f, 12.0f), juce::Justification::centred);
        }
        for (float db : { -18.0f, -12.0f, -6.0f, 0.0f, 6.0f, 12.0f, 18.0f })
        {
            g.setColour (db == 0.0f ? theme::track : theme::edge);
            g.drawHorizontalLine ((int) yFor (db), plot.getX(), plot.getRight());
            g.setColour (theme::dim);
            g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (plot.getX() + 2.0f, yFor (db) - 12.0f, 30.0f, 12.0f),
                        juce::Justification::centredLeft);
        }

        // spectrum (post EQ), tilted +4.5 dB/oct so a balanced mix looks flat
        {
            juce::Path spec;
            spec.startNewSubPath (plot.getX(), plot.getBottom());
            for (float x = plot.getX(); x <= plot.getRight(); x += 2.0f)
            {
                const float f = freqFor (x);
                const float bin = f / (float) fs * (float) fftSize;
                const int b0 = juce::jlimit (1, (int) postDb.size() - 2, (int) bin);
                const float frac = bin - (float) b0;
                const float db = postDb[(size_t) b0] + frac * (postDb[(size_t) b0 + 1] - postDb[(size_t) b0]) + 4.5f * std::log2 (f / 1000.0f);
                spec.lineTo (x, juce::jlimit (plot.getY(), plot.getBottom(), plot.getBottom() - plot.getHeight() * (db + 84.0f) / 84.0f));
            }
            spec.lineTo (plot.getRight(), plot.getBottom());
            spec.closeSubPath();
            g.setColour (theme::text.withAlpha (0.08f));
            g.fillPath (spec);
        }

        // curves
        const auto manual = curvePoints ([this] (double f) { return manualDb (f); });
        if (smartView)
        {
            drawCurve (g, manual, theme::accent.withAlpha (0.35f), 1.5f, false);
            const auto carveCurve = curvePoints ([this] (double f) { return carveDb (f); });
            drawCurve (g, carveCurve, theme::blue, 1.5f, true);
            for (int s = 0; s < EqProcessor::numSmart; ++s)
            {
                const auto st = proc.readSmart (s);
                if (! st.on) continue;
                const auto maxCut = curvePoints ([&] (double f) { return vs::BiquadCoefs::peak (fs, st.freq, st.q, -st.rangeDb).magnitudeDb (f, fs); });
                drawCurve (g, maxCut, theme::red.withAlpha (0.35f), 1.0f, false);
                const float now = proc.smartCutDb[s].load();
                if (now > 0.05f)
                    drawCurve (g, curvePoints ([&] (double f) { return vs::BiquadCoefs::peak (fs, st.freq, st.q, -now).magnitudeDb (f, fs); }),
                               theme::red, 2.0f, true);
            }
            drawCurve (g, curvePoints ([&] (double f) { return manualDb (f) + carveDb (f) + smartNowDb (f); }), theme::text, 1.5f, false);
            drawSmartNodes (g);
            drawResonances (g);
        }
        else
        {
            for (int b = 0; b < EqProcessor::numBands; ++b)
            {
                const auto st = proc.readBand (b);
                if (st.on && b == selectedBand)
                    drawCurve (g, curvePoints ([&] (double f) { return vs::EqBand::magnitudeDb (st, f, fs); }), bandColour (b).withAlpha (0.8f), 1.0f, true);
            }
            drawCurve (g, manual, theme::accent, 2.5f, false);
            drawBandNodes (g);
        }

        g.setColour (theme::dim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (smartView ? "drag a smart node: left/right = frequency, up/down = how deep it can cut. Click a + marker to tame that resonance."
                              : "drag nodes to shape, scroll for width (Q), double-click to switch a band on/off",
                    plot.withTrimmedTop (2.0f).removeFromTop (14.0f), juce::Justification::centredRight);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.position;
        if (smartView)
        {
            for (auto& res : resonances)
                if (p.getDistanceFrom ({ xFor (res.freq), plot.getY() + 22.0f }) < 12.0f)
                {
                    if (onResonanceClicked) onResonanceClicked (res);
                    return;
                }
            dragIndex = nearestNode (p, true);
            if (dragIndex >= 0)
            {
                selectedSmart = dragIndex;
                if (onSelectSmart) onSelectSmart (dragIndex);
                begin ({ id ("s", dragIndex, "freq"), id ("s", dragIndex, "range") });
            }
        }
        else
        {
            dragIndex = nearestNode (p, false);
            if (dragIndex >= 0)
            {
                selectedBand = dragIndex;
                if (onSelectBand) onSelectBand (dragIndex);
                begin ({ id ("b", dragIndex, "freq"), id ("b", dragIndex, "gain") });
            }
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragIndex < 0) return;
        const float f = juce::jlimit (20.0f, 20000.0f, freqFor (e.position.x));
        if (smartView)
        {
            set (id ("s", dragIndex, "freq"), juce::jlimit (40.0f, 18000.0f, f));
            set (id ("s", dragIndex, "range"), juce::jlimit (0.0f, 18.0f, -dbFor (e.position.y)));
        }
        else
        {
            set (id ("b", dragIndex, "freq"), f);
            const int type = proc.readBand (dragIndex).type;
            if (type <= vs::EqBand::highShelf)
                set (id ("b", dragIndex, "gain"), juce::jlimit (-18.0f, 18.0f, dbFor (e.position.y)));
        }
    }

    void mouseUp (const juce::MouseEvent&) override { endAll(); dragIndex = -1; }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int i = nearestNode (e.position, smartView);
        if (i < 0) return;
        const auto pid = id (smartView ? "s" : "b", i, "on");
        auto* param = proc.apvts.getParameter (pid);
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
        param->endChangeGesture();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        int i = nearestNode (e.position, smartView, 40.0f);
        if (i < 0) i = smartView ? selectedSmart : selectedBand;
        const auto pid = id (smartView ? "s" : "b", i, "q");
        auto* param = proc.apvts.getParameter (pid);
        const float q = param->convertFrom0to1 (param->getValue()) * std::exp (w.deltaY * 1.2f);
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (q));
        param->endChangeGesture();
    }

private:
    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder, histSize = 1 << 13;

    static juce::String id (const char* prefix, int i, const char* what) { return juce::String (prefix) + juce::String (i + 1) + "_" + what; }

    float xFor (float f) const { return plot.getX() + plot.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); }
    float freqFor (float x) const { return 20.0f * std::pow (1000.0f, (x - plot.getX()) / plot.getWidth()); }
    float yFor (float db) const { return plot.getCentreY() - plot.getHeight() * 0.5f * db / 24.0f; }
    float dbFor (float y) const { return (plot.getCentreY() - y) / (plot.getHeight() * 0.5f) * 24.0f; }

    // Filter designs are computed once per frame, then evaluated per pixel
    struct CachedFilter { std::array<vs::BiquadCoefs, 4> c; int n = 0; };
    CachedFilter bandCache[EqProcessor::numBands];
    std::vector<vs::BiquadCoefs> carveCache;

    void refreshCaches()
    {
        for (int b = 0; b < EqProcessor::numBands; ++b)
        {
            const auto st = proc.readBand (b);
            bandCache[b].n = st.on ? vs::EqBand::design (st, fs, bandCache[b].c) : 0;
        }
        carveCache.clear();
        for (int k = 0; k < vs::Carve::numBands; ++k)
        {
            const float gdb = proc.carveGainDb[k].load();
            if (gdb < -0.05f) carveCache.push_back (vs::BiquadCoefs::peak (fs, vs::Carve::bandFreq (k), 2.8, gdb * 0.8f));
        }
    }

    double manualDb (double f) const
    {
        double db = 0.0;
        for (auto& b : bandCache)
            for (int i = 0; i < b.n; ++i) db += b.c[(size_t) i].magnitudeDb (f, fs);
        return db;
    }

    double carveDb (double f) const
    {
        double db = 0.0;
        for (auto& c : carveCache) db += c.magnitudeDb (f, fs);
        return db;
    }

    double smartNowDb (double f) const
    {
        double db = 0.0;
        for (int s = 0; s < EqProcessor::numSmart; ++s)
        {
            const auto st = proc.readSmart (s);
            const float now = proc.smartCutDb[s].load();
            if (st.on && now > 0.05f) db += vs::BiquadCoefs::peak (fs, st.freq, st.q, -now).magnitudeDb (f, fs);
        }
        return db;
    }

    template <typename Fn>
    std::vector<juce::Point<float>> curvePoints (Fn fn) const
    {
        std::vector<juce::Point<float>> pts;
        for (float x = plot.getX(); x <= plot.getRight(); x += 2.0f)
            pts.push_back ({ x, juce::jlimit (plot.getY(), plot.getBottom(), yFor ((float) fn ((double) freqFor (x)))) });
        return pts;
    }

    void drawCurve (juce::Graphics& g, const std::vector<juce::Point<float>>& pts, juce::Colour c, float thickness, bool fillToZero)
    {
        if (pts.empty()) return;
        juce::Path p;
        p.startNewSubPath (pts.front());
        for (auto& pt : pts) p.lineTo (pt);
        if (fillToZero)
        {
            juce::Path area (p);
            area.lineTo (pts.back().x, yFor (0.0f));
            area.lineTo (pts.front().x, yFor (0.0f));
            area.closeSubPath();
            g.setColour (c.withAlpha (0.18f));
            g.fillPath (area);
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (thickness));
    }

    juce::Point<float> bandNode (int b) const
    {
        const auto st = proc.readBand (b);
        const float y = st.type <= vs::EqBand::highShelf ? yFor (st.gainDb) : yFor (0.0f);
        return { xFor (st.freq), y };
    }

    juce::Point<float> smartNode (int s) const
    {
        const auto st = proc.readSmart (s);
        return { xFor (st.freq), yFor (-st.rangeDb) };
    }

    int nearestNode (juce::Point<float> p, bool smart, float maxDist = 14.0f) const
    {
        int best = -1;
        float bestD = maxDist;
        const int count = smart ? EqProcessor::numSmart : EqProcessor::numBands;
        for (int i = 0; i < count; ++i)
        {
            const float d = p.getDistanceFrom (smart ? smartNode (i) : bandNode (i));
            if (d < bestD) { bestD = d; best = i; }
        }
        return best;
    }

    void drawBandNodes (juce::Graphics& g)
    {
        for (int b = 0; b < EqProcessor::numBands; ++b)
        {
            const auto st = proc.readBand (b);
            const auto pt = bandNode (b);
            const float rad = b == selectedBand ? 9.0f : 7.0f;
            const auto c = bandColour (b);
            if (st.on) { g.setColour (c); g.fillEllipse (pt.x - rad, pt.y - rad, rad * 2, rad * 2); }
            g.setColour (st.on ? vs::theme::bg : c);
            if (! st.on) g.drawEllipse (pt.x - rad, pt.y - rad, rad * 2, rad * 2, 1.5f);
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            g.drawText (juce::String (b + 1), juce::Rectangle<float> (pt.x - rad, pt.y - rad, rad * 2, rad * 2), juce::Justification::centred);
            if (b == selectedBand)
            {
                g.setColour (vs::theme::text);
                g.drawEllipse (pt.x - rad - 2, pt.y - rad - 2, rad * 2 + 4, rad * 2 + 4, 1.0f);
            }
        }
    }

    void drawSmartNodes (juce::Graphics& g)
    {
        for (int s = 0; s < EqProcessor::numSmart; ++s)
        {
            const auto st = proc.readSmart (s);
            const auto pt = smartNode (s);
            juce::Path d;
            const float rad = s == selectedSmart ? 9.0f : 7.5f;
            d.addQuadrilateral (pt.x, pt.y - rad, pt.x + rad, pt.y, pt.x, pt.y + rad, pt.x - rad, pt.y);
            g.setColour (st.on ? vs::theme::red : vs::theme::dim);
            if (st.on) g.fillPath (d); else g.strokePath (d, juce::PathStrokeType (1.5f));
            g.setColour (st.on ? vs::theme::bg : vs::theme::dim);
            g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
            g.drawText ("S" + juce::String (s + 1), juce::Rectangle<float> (pt.x - rad, pt.y - rad, rad * 2, rad * 2), juce::Justification::centred);
        }
    }

    void drawResonances (juce::Graphics& g)
    {
        if (! findResonances) return;
        for (auto& res : resonances)
        {
            const float x = xFor (res.freq), y = plot.getY() + 22.0f;
            g.setColour (vs::theme::red.withAlpha (0.25f));
            g.drawVerticalLine ((int) x, y + 8.0f, plot.getBottom());
            g.setColour (vs::theme::red);
            g.fillEllipse (x - 8.0f, y - 8.0f, 16.0f, 16.0f);
            g.setColour (vs::theme::bg);
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawText ("+", juce::Rectangle<float> (x - 8.0f, y - 8.5f, 16.0f, 16.0f), juce::Justification::centred);
        }
    }

    // ---- analysis -------------------------------------------------------
    void pullTap (EqProcessor::Tap& tap, std::vector<float>& hist, int& write)
    {
        const int got = tap.pull (tmp.data(), (int) tmp.size());
        for (int i = 0; i < got; ++i)
        {
            hist[(size_t) write] = tmp[(size_t) i];
            write = (write + 1) & (histSize - 1);
        }
    }

    void spectrum (const std::vector<float>& hist, int write)
    {
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        for (int i = 0; i < fftSize; ++i)
            fftData[(size_t) i] = hist[(size_t) ((write - fftSize + i) & (histSize - 1))] * window[(size_t) i];
        fft.performFrequencyOnlyForwardTransform (fftData.data(), true);
    }

    void detectResonances() { resonances = vs::findResonances (prePow, fs, fftSize); }

    void begin (std::initializer_list<juce::String> ids)
    {
        endAll();
        for (auto& i : ids)
            if (auto* p = proc.apvts.getParameter (i)) { p->beginChangeGesture(); gestures.push_back (p); }
    }
    void endAll() { for (auto* p : gestures) p->endChangeGesture(); gestures.clear(); }
    void set (const juce::String& pid, float v)
    {
        if (auto* p = proc.apvts.getParameter (pid)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    }

    EqProcessor& proc;
    double fs = 48000.0;
    juce::Rectangle<float> plot;
    juce::dsp::FFT fft { fftOrder };
    std::vector<float> preHist, postHist, fftData, window, postDb, prePow, tmp;
    int preWrite = 0, postWrite = 0, frame = 0, dragIndex = -1;
    std::vector<juce::RangedAudioParameter*> gestures;
};
