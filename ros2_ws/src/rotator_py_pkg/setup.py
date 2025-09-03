#!/usr/bin/env python3
#
# Eagle-Eye-AI
# Smart Following Camera with Face Recognition
#   for Kria KR260 Board
#
# Created by: Matjaz Zibert S59MZ - July 2024
#
# Rotator Controller Python Package
#   - Controls the Pan-Tilt Camera Rotator via Pelco-D protocol
#
# Design based on Kria KV260 Smartcam Demo App by AMD
#
# Hackster.io Project link:
#     https://www.hackster.io/matjaz4
#


from setuptools import find_packages, setup

package_name = 'rotator_py_pkg'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='root',
    maintainer_email='root@todo.todo',
    description='TODO: Package description',
    license='TODO: License declaration',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            "rotator_controller = rotator_py_pkg.rotator_controller:main",
            "camera_controller = rotator_py_pkg.camera_controller:main",
            "led_controller = rotator_py_pkg.led_controller:main",
            "btn_controller = rotator_py_pkg.btn_controller:main"
        ],
    },
)
