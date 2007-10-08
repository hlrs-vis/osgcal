@echo off

cd cal3D_export

python csv_edit.py --create-if-not-exists %1_textures.csv --sync-no-overwrite %1_current_textures.csv
del %1_current_textures.csv

IF ERRORLEVEL 2 goto error

python export_textures.py --output-dir %2 --textures-list %1_textures.csv --paths-list %1_map_paths.lst --delete-other-textures --changed-only

IF ERRORLEVEL 2 goto error

del %1_map_paths.lst

osgCalPreparer %2\cal3d.cfg

IF ERRORLEVEL 2 goto error

cd ../

exit 0

:error
echo Export failed!
pause