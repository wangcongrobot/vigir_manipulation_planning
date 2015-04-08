import os
import rospy
import rospkg

from math import pi

from qt_gui.plugin import Plugin
from python_qt_binding import loadUi
from python_qt_binding.QtGui import QWidget
from std_msgs.msg import String
from vigir_planning_msgs.msg import HeadControlCommand



class HeadControl(Plugin):

    def __init__(self, context):
        super(HeadControl, self).__init__(context)
        # Give QObjects reasonable names
        self.setObjectName('Head Control')

        #Publisher
        self.modePublisher = rospy.Publisher('/thor_mang/head_control_mode', HeadControlCommand)

        # Process standalone plugin command-line arguments
        from argparse import ArgumentParser
        parser = ArgumentParser()
        # Add argument(s) to the parser.
        parser.add_argument("-q", "--quiet", action="store_true",
                      dest="quiet",
                      help="Put plugin in silent mode")
        args, unknowns = parser.parse_known_args(context.argv())
        if not args.quiet:
            print 'arguments: ', args
            print 'unknowns: ', unknowns

        # Create QWidget
        self._widget = QWidget()
        # Get path to UI file which should be in the "resource" folder of this package
        ui_file = os.path.join(rospkg.RosPack().get_path('vigir_head_control_widget'), 'resource', 'head_control_widget.ui')
        # Extend the widget with all attributes and children from UI file
        loadUi(ui_file, self._widget)
        # Give QObjects reasonable names
        self._widget.setObjectName('HeadControlUi')
        # Show _widget.windowTitle on left-top of each plugin (when
        # it's set in _widget). This is useful when you open multiple
        # plugins at once. Also if you open multiple instances of your
        # plugin at once, these lines add number to make it easy to
        # tell from pane to pane.
        if context.serial_number() > 1:
            self._widget.setWindowTitle(self._widget.windowTitle() + (' (%d)' % context.serial_number()))
        # Add widget to the user interface
        context.add_widget(self._widget)

        # Connect buttons
        self._widget.modeTrackLeftRadioButton.pressed.connect(self.modeTrackLeftRadioButtonPressed)
        self._widget.modeTrackRightRadioButton.pressed.connect(self.modeTrackRightRadioButtonPressed)
        self._widget.zeroPushButton.pressed.connect(self.zeroPushButtonPressed)

        self._widget.panDial.sliderReleased.connect(self.manualJointChanged)
        self._widget.panSlider.sliderReleased.connect(self.manualJointChanged)
        self._widget.panSpinBox.editingFinished.connect(self.manualJointChanged)
        self._widget.tiltSlider.sliderReleased.connect(self.manualJointChanged)
        self._widget.tiltSpinBox.editingFinished.connect(self.manualJointChanged)



    def modeTrackLeftRadioButtonPressed(self):
        command = HeadControlCommand(HeadControlCommand.TRACK_LEFT_HAND, [])
        self.modePublisher.publish(command)

    def modeTrackRightRadioButtonPressed(self):
        command = HeadControlCommand(HeadControlCommand.TRACK_RIGHT_HAND, [])
        self.modePublisher.publish(command)

    def zeroPushButtonPressed(self):
        command = HeadControlCommand(HeadControlCommand.USE_PROVIDED_JOINTS, [0.0, 0.0])
        self.modePublisher.publish(command)

    def manualJointChanged(self):
        pan = float(self._widget.panSpinBox.value()) / 360.0 * 2 * pi
        tilt = float(self._widget.tiltSpinBox.value()) / 360.0 * 2 * pi
        command = HeadControlCommand(HeadControlCommand.USE_PROVIDED_JOINTS, [pan, tilt])
        self.modePublisher.publish(command)


    def shutdown_plugin(self):
        # TODO unregister all publishers here
        pass

    def save_settings(self, plugin_settings, instance_settings):
        # TODO save intrinsic configuration, usually using:
        # instance_settings.set_value(k, v)
        pass

    def restore_settings(self, plugin_settings, instance_settings):
        # TODO restore intrinsic configuration, usually using:
        # v = instance_settings.value(k)
        pass

    #def trigger_configuration(self):
        # Comment in to signal that the plugin has a way to configure
        # This will enable a setting button (gear icon) in each dock widget title bar
        # Usually used to open a modal configuration dialog