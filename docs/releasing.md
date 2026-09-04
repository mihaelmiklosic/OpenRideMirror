# Release checklist

OpenRideMirror publishes source, not prebuilt firmware or Garmin applications.

1. Update `CHANGELOG.md` and confirm protocol compatibility.
2. Run `./orm protocol generate` and review generated diffs.
3. Run `./orm release check`.
4. Run `./orm build garmin --all` locally.
5. Flash demo mode and inspect every UI state on the reference display.
6. Flash live mode and test discovery after clearing neither MAC nor bonds.
7. Test cycling and walking with a real GPS fix.
8. Verify activity, GPS and extended data, temperature, Push view and saved history.
9. Confirm no `.prg`, keys, build output, private paths, personal GPX/FIT data or downloaded OSM cache is tracked.
10. Confirm visible OpenStreetMap attribution for any distributed generated map.
11. Tag the reviewed source commit and let users build locally.

GitHub Actions validates source-level tests and both ESP modes. Garmin builds remain a local check because the Connect IQ SDK distribution and developer key are not committed.
