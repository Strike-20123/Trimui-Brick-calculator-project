# Brick Calculator — build instructions (no Docker, no terminal)

This repo builds itself. Once these files are uploaded to GitHub, GitHub's
own servers compile the app for you and hand you back a finished folder to
copy onto your SD card.

## What to do

1. Go to the **Actions** tab at the top of this repo.
2. You should see a run called **"Build Calculator"** already in progress
   (or about to start). Click it.
3. Wait. The first run builds a whole cross-compiler from scratch, so it can
   take 10-20 minutes. You'll see a spinning yellow dot; it turns into a
   green check (success) or a red X (failed) when done.
4. If it's green: scroll to the bottom of that run's page to
   **Artifacts** and click **Calculator** to download a zip.
5. Unzip it. Copy the **Calculator** folder inside onto your SD card at
   `Apps/Calculator` (so the path is `Apps/Calculator/config.json`, etc.).
6. Put the SD card back in the Brick and open **Calculator** from the Apps
   menu.

## If it's red (failed)

Click the failed run, click the red step to expand it, and copy the error
text. Paste that back to whoever helped you set this up — it's a real error
from a real build attempt, which is much easier to fix than guessing blind.
