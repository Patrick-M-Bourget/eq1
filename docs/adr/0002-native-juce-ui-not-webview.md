# Native JUCE graphics for the UI, not a WebView

The UI is drawn with JUCE's native graphics rather than an HTML/JS WebView UI (supported since JUCE 8). A 60 fps Analyzer with draggable Bands and Spectrum Grab is the core of the experience, and native drawing gives the most predictable performance across hosts without a JS↔C++ bridge for every parameter or per-host WebView quirks. We gave up faster iteration with web tooling to get this.
