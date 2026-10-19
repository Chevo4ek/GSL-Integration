# Changelog

## v0.0.3
<co>Work in progress — not released yet.</c>

- Future Classic is now called <cg>Upcoming Levels</c> everywhere (short tag <cg>UL</c>)
- The updater refuses a release file that isn't actually a newer version

## v0.0.2
<co>Work in progress — not released yet.</c>

- Updates now come from the public GSL-Integration-Releases repository

## v0.0.1
<co>Work in progress — not released yet.</c>

### Level search and level page
- List positions under the difficulty face (<cy>#1 CL</c> or <cy>#1 Classic</c>)
- Demon faces and difficulty from the list
- List rate (Featured, Epic, ...) shown as the face frame and as text on the level page
- List points in the level stats
- Long song names no longer go over the level preview
- Works with the More Difficulties mod: its Casual/Tough/Cruel face no longer covers the list's demon face
- Round GSL button in the level search screen

### GSL screen
- Real level cards, 10 per page, sorted by list position
- Level screenshots in the background, smoothly changing every 30 seconds
- The level list always has its own blue background and separator lines (doesn't depend on texture packs)
- Fixed: blurred background showed blue edges and corners
- Background screenshots in full resolution (they looked blurry before)
- Search by level name — search bar is part of the list frame, like in the game's own lists
- The list frame is no longer recreated on every page, so the search text stays
- Search bar smoothly hides when you scroll down; a small arrow opens and closes it
- Square list picker button with a trophy (Classic, Challenge, Spam, Upcoming Levels), trophy and name centered
- Go to page, randomizer and last page buttons
- Filters like on the site: Category (Main / Extended / Advanced / Unbounded), Length, Only new, Quality, Sort by, plus Completed by and Created by
- Info button with the site description and a link to globalshittylist.com
- Upcoming Levels: extra note in the info window and an arrow pointing to it until it's read

### Settings
- Sections: Display, List screen, Sync
- Rank names (short / full), list rate, song name trimming
- Default list, GSL button on/off with a reminder when it's hidden
- Search bar mode: hide on scroll, arrow only or always shown
- Background: blur, darkening and how often the screenshot changes
- Check interval and notifications about list updates

### Platforms
- Android support (64-bit and 32-bit) next to Windows

### Updates
- The mod checks for a new version on game start and installs it in one click (verified by checksum, applied after restart)

### Sync
- Automatic sync with globalshittylist.com, list cached on disk
