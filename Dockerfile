FROM debian:bookworm-slim

# Enable 32-bit architecture for wine32
RUN dpkg --add-architecture i386

RUN apt-get update && apt-get install -y --no-install-recommends \
    wine \
    wine32 \
    xvfb \
    x11vnc \
    scrot \
    imagemagick \
    xdotool \
    procps \
    && rm -rf /var/lib/apt/lists/*

COPY entrypoint.sh screenshot.sh send-input.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/entrypoint.sh \
               /usr/local/bin/screenshot.sh \
               /usr/local/bin/send-input.sh

ENV WINEPREFIX=/wine-prefix
ENV WINEARCH=win32
# Force Wine's builtin ddraw — DDrawCompat (ddraw.dll in the game dir) crashes Wine
ENV WINEDLLOVERRIDES=ddraw=b
ENV DISPLAY=:99

EXPOSE 5900

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
