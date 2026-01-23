import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution, Command
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os
import xacro
from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
def generate_launch_description():
    robot_name = "differential_drive_robot"
    package_name = "gazebo_differential_drive_robot"
    pkg_bme_gazebo_sensors = get_package_share_directory('bme_gazebo_sensors')
    pkg_gazebo_differential_drive_robot = get_package_share_directory('gazebo_differential_drive_robot')
    pkg_erl1 = get_package_share_directory('erl1')

    gazebo_models_path, ignore_last_dir = os.path.split(pkg_bme_gazebo_sensors)
    os.environ["GZ_SIM_RESOURCE_PATH"] += os.pathsep + gazebo_models_path

    gazebo_models_path, ignore_last_dir = os.path.split(pkg_gazebo_differential_drive_robot)
    os.environ["GZ_SIM_RESOURCE_PATH"] += os.pathsep + gazebo_models_path

    rviz_launch_arg = DeclareLaunchArgument(
        'rviz', default_value='true',
        description='Open RViz'
    )

    rviz_config_arg = DeclareLaunchArgument(
        'rviz_config', default_value='rviz.rviz',
        description='RViz config file'
    )

    world_arg = DeclareLaunchArgument(
        'world', default_value='my_world.sdf',
        description='Name of the Gazebo world file to load'
    )
    

    # model_arg = DeclareLaunchArgument(
    #     'model', default_value='mogi_bot.urdf',
    #     description='Name of the URDF description to load'
    # )

    x_arg = DeclareLaunchArgument(
        'x', default_value='0.0', description='Initial X position')

    y_arg = DeclareLaunchArgument(
        'y', default_value='0.0', description='Initial Y position')

    z_arg = DeclareLaunchArgument(
        'z', default_value='0.5', description='Initial Z position')

    roll_arg = DeclareLaunchArgument(
        'R', default_value='0.0', description='Initial Roll')

    pitch_arg = DeclareLaunchArgument(
        'P', default_value='0.0', description='Initial Pitch')

    yaw_arg = DeclareLaunchArgument(
        'Y', default_value='0.0', description='Initial Yaw')

    x = LaunchConfiguration('x')
    y = LaunchConfiguration('y')
    z = LaunchConfiguration('z')
    roll = LaunchConfiguration('R')
    pitch = LaunchConfiguration('P')
    yaw = LaunchConfiguration('Y')

    robot_model_path = os.path.join(
        get_package_share_directory('gazebo_differential_drive_robot'),
        'model',
        'robot.xacro'
    )
    robot_description = xacro.process_file(robot_model_path).toxml()
    

    sim_time_arg = DeclareLaunchArgument(
        'use_sim_time', default_value='True',
        description='Flag to enable use_sim_time'
    )

    # Define the path to your URDF or Xacro file
    urdf_file_path = PathJoinSubstitution([
        pkg_gazebo_differential_drive_robot,  # Replace with your package name
        "model",
        'robot.xacro'
    ])

    world_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_erl1, 'launch', 'my_launch.py'),
        ),
        launch_arguments={
        'world': LaunchConfiguration('world'),
        }.items()
    )

    # Launch rviz
    rviz_node = Node(
        package='rviz2',
        executable='rviz2',
        arguments=['-d', PathJoinSubstitution([pkg_bme_gazebo_sensors, 'rviz', LaunchConfiguration('rviz_config')])],
        condition=IfCondition(LaunchConfiguration('rviz')),
        parameters=[
            {'use_sim_time': LaunchConfiguration('use_sim_time')},
        ]
    )

    # Spawn the URDF model using the `/world/<world_name>/create` service
    # spawn_urdf_node = Node(
    #     package="ros_gz_sim",
    #     executable="create",
    #     arguments=[
    #         "-name", "diff_bot",
    #         "-topic", "robot_description",
    #         "-x", LaunchConfiguration('x'), "-y", LaunchConfiguration('y'), "-z", "0.5", "-Y", LaunchConfiguration('yaw')  # Initial spawn position
    #     ],
    #     output="screen",
    #     parameters=[
    #         {'use_sim_time': LaunchConfiguration('use_sim_time')},
    #     ]
    # )
    spawn_urdf_node = Node(
        package='ros_gz_sim',
        executable='create',
        arguments=[
            '-name', robot_name,
            '-string', robot_description,
            '-x', x,
            '-y', y,
            '-z', z,
            '-R', roll,
            '-P', pitch,
            '-Y', yaw,
            '-allow_renaming', 'false'
        ],
        output='screen',
        parameters=[
             {'use_sim_time': LaunchConfiguration('use_sim_time')},
         ]
    )
    # Node to bridge /cmd_vel and /odom
    # gz_bridge_node = Node(
    #     package="ros_gz_bridge",
    #     executable="parameter_bridge",
    #     arguments=[
    #         "/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock",
    #         "/cmd_vel@geometry_msgs/msg/Twist@gz.msgs.Twist",
    #         "/odom@nav_msgs/msg/Odometry@gz.msgs.Odometry",
    #         "/joint_states@sensor_msgs/msg/JointState@gz.msgs.Model",
    #     #    "/tf@tf2_msgs/msg/TFMessage@gz.msgs.Pose_V",
    #         # "/camera/image@sensor_msgs/msg/Image@gz.msgs.Image",
    #         "/camera/camera_info@sensor_msgs/msg/CameraInfo@gz.msgs.CameraInfo",
    #         "imu@sensor_msgs/msg/Imu@gz.msgs.IMU",
    #         "/navsat@sensor_msgs/msg/NavSatFix@gz.msgs.NavSat",
    #         "/scan@sensor_msgs/msg/LaserScan@gz.msgs.LaserScan",
    #         "/odom@"

    #     ],
    #     output="screen",
    #     parameters=[
    #         {'use_sim_time': LaunchConfiguration('use_sim_time')},
    #     ]
    # )
    gz_bridge_params_path = os.path.join(
        get_package_share_directory(package_name),
        'config',
        'gz_bridge.yaml'
    )
    gz_bridge_node = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        arguments=[
            '--ros-args', '-p',
            f'config_file:={gz_bridge_params_path}'
        ],
        output='screen'
    )

    # robot_state_publisher_node = Node(
    #     package='robot_state_publisher',
    #     executable='robot_state_publisher',
    #     name='robot_state_publisher',
    #     output='screen',
    #     parameters=[
    #         {'robot_description': Command(['xacro', ' ', urdf_file_path]),
    #          'use_sim_time': LaunchConfiguration('use_sim_time')},
    #     ],
    #     remappings=[
    #         ('/tf', 'tf'),
    #         ('/tf_static', 'tf_static')
    #     ]
    # )
    robot_state_publisher_node = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[
            {'robot_description': robot_description, 'use_sim_time': LaunchConfiguration('use_sim_time')}
            # {'robot_description': Command(['xacro', ' ', urdf_file_path]),
            #  'use_sim_time': LaunchConfiguration('use_sim_time')},
        ],
        remappings=[
            ('/tf', 'tf'),
            ('/tf_static', 'tf_static')
        ]
    )

    ekf_node = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        output='screen',
        parameters=[
            os.path.join(pkg_bme_gazebo_sensors, 'config', 'ekf.yaml'),
            {'use_sim_time': LaunchConfiguration('use_sim_time')},
            ]
)
    gz_image_bridge_node = Node(
        package="ros_gz_image",
        executable="image_bridge",
        arguments=[
        "/camera/image",
        ],
        output="screen",
        parameters=[
        {'use_sim_time': LaunchConfiguration('use_sim_time'),
        'camera.image.compressed.jpeg_quality': 75},
        ],
        )


    launchDescriptionObject = LaunchDescription()
    
    launchDescriptionObject.add_action(rviz_launch_arg)
    launchDescriptionObject.add_action(rviz_config_arg)
    launchDescriptionObject.add_action(world_arg)
    # launchDescriptionObject.add_action(model_arg)
    launchDescriptionObject.add_action(x_arg)
    launchDescriptionObject.add_action(y_arg)
    launchDescriptionObject.add_action(yaw_arg)
    launchDescriptionObject.add_action(roll_arg)
    launchDescriptionObject.add_action(pitch_arg)
    launchDescriptionObject.add_action(sim_time_arg)
    launchDescriptionObject.add_action(world_launch)
    launchDescriptionObject.add_action(rviz_node)
    launchDescriptionObject.add_action(spawn_urdf_node)
    launchDescriptionObject.add_action(gz_bridge_node)
    launchDescriptionObject.add_action(robot_state_publisher_node)
    launchDescriptionObject.add_action(gz_image_bridge_node)
    launchDescriptionObject.add_action(ekf_node)

    return launchDescriptionObject