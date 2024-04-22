# melibu_decoder

This repository contains both low level and high level analyzer for MeLiBu decoding.

## Install python packages

The MeLiBu decoder requires to install a set of python packages for MBDF file parser. 
On Windows these have to be installed via PowerShell by using the following commands.
Please start the PowerShell as admin and use command cd folder path to go to melibu_decoder folder. Now do following two steps as expalined:

1. get a list of all Melexis python wheels
```
$WHEEL_FILES = get-childitem *.whl
```

2. install Melexis python wheels to the SALEAE logic folder, please update the target folder according to your installation!
```
pip install --target "C:\Program Files\Logic\resources\windows-x64\pythonlibs\lib\site-packages" pathlib $WHEEL_FILES
```

## Using analyzers

To use both analyzers follow steps in theirs README to build and import them to Logic app. Check user manual to see detailed information about importing them and to see how to use all features.