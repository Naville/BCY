from setuptools import setup,find_packages
import os,sys,errno
def recursiveSearch(directory):
    return [os.path.join(root, name)
                 for root, dirs, files in os.walk(directory)
                 for name in files
                 if name.endswith(".DS_Store")==False]

ChromeExtensionInstallPath=None
if os.name == 'nt':
    ChromeExtensionInstallPath = os.path.join(os.path.join(os.environ['USERPROFILE']),'PyBCYChromeExtension')
else:
    ChromeExtensionInstallPath = os.path.join(os.path.join(os.path.expanduser('~')),'PyBCYChromeExtension')

try:
    os.makedirs(ChromeExtensionInstallPath)
except OSError as exception:
    if exception.errno != errno.EEXIST:
        raise
setup(
    name='PyBCY',
    version='1.7.8',
    packages=find_packages(),
    url = "https://github.com/Naville/PyBCY",
    license='GPL',
    author='Naville',
    scripts=['PyBCY/bin/PyBCYDownloader.py'],
    author_email='admin@mayuyu.io',
    description='Python Interface For BCY.net API',
    install_requires=['pkcs7','PyCrypto',"requests"],
    zip_safe = False,
    include_package_data=True,
    data_files=[(ChromeExtensionInstallPath, recursiveSearch('ChromeExtension/'))]
)
