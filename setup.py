from setuptools import setup,find_packages
import os,sys,errno
import unittest
def my_test_suite(): #From https://stackoverflow.com/questions/17001010/how-to-run-unittest-discover-from-python-setup-py-test
    test_loader = unittest.TestLoader()
    test_suite = test_loader.discover('UnitTest', pattern='Test.py')
    return test_suite
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
    version='2.0.0',
    packages=find_packages(),
    url = "https://github.com/Naville/PyBCY",
    license='GPL',
    author='Naville',
    author_email='admin@mayuyu.io',
    description='Python Interface For BCY.net API',
    install_requires=['pkcs7','PyCrypto',"requests"],
    test_suite='setup.my_test_suite',
)
