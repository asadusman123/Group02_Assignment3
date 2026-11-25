#!/usr/bin/env bash
# build.sh - regenerate Group02_report.pdf from report.md + appendix.md
# needs: pandoc and google-chrome (or chromium); run from report/
set -euo pipefail
CHROME=$(command -v google-chrome || command -v chromium || command -v chromium-browser)
pandoc report.md appendix.md -s --css style.css --embed-resources -o report.html
"$CHROME" --headless=new --no-sandbox --disable-gpu --no-pdf-header-footer --print-to-pdf=Group02_report.pdf report.html
rm -f report.html
echo "wrote Group02_report.pdf"
