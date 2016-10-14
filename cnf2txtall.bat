@echo off

echo Starting bach processing...
for %%i in (*.cnf) do (
    echo %%i
    cnf2txt.exe "%%i"
)
echo End batch processing
