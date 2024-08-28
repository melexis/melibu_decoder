# Install python packages

On Windows these have to be installed via PowerShell by using the following commands.
Follow steps:

1. Start the PowerShell as admin
2. Use command cd folder_path to go to this folder (...\MeLiBu_high_level\python_mbdf_parser)
3. Install Melexis python wheels to the SALEAE logic folder. Please update the target folder according to your installation!
```
pip install --target "C:\Program Files\Logic\resources\windows-x64\pythonlibs\lib\site-packages" -r py-requirements.txt
```
