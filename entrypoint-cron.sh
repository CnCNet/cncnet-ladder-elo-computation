#!/bin/sh
set -e

if [ -f /mnt/crontab ]; then
    cp /mnt/crontab /etc/cron.d/elogen-cron
    chmod 644 /etc/cron.d/elogen-cron
    chown root:root /etc/cron.d/elogen-cron
else
    echo "No crontab found at /mnt/crontab"
fi

# Do not exit container, but start cron in any case.
exec cron -f -L 2
