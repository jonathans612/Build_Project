Notes for documentation and for future MQTT broker implementations

-- docker-compose.yml explanation --

Ports format follows
    
    host_port:container_port

It maps the port of the computer hosting the container to the port of the container 
that the broker uses. Mapping 1883:1883 just sets the same port on both, 1883 being
the standard port MQTT generally uses

Volumes format follows

    host_docker_directory:container_directory

It mounts the directory specified to the container, mapping the mounts to the directories
specified after the colon. We will be able to access any output data in log/ and data/, 
with config/ serving as the directory in which we will provide to the container the 
mosquitto.conf file -- this file configures the broker to our use case.

-- mosquitto.conf explanation --

Documentation for mosquitto configuration: 
https://mosquitto.org/man/mosquitto-conf-5.html

    connection_messages true

This is a debugging directive. It tells the broker to write a log message every time a client connects or disconnects. This is extremely useful for seeing in real-time if your microcontroller or laptop is successfully authenticating.

    log_dest stdout

This directive controls where the logs are sent. stdout (standard output) is the best practice for Docker containers. It pipes all broker logs directly to the container's log, which you can then access from your host machine by running docker logs <container_name>.

    log_timestamp true

This adds a timestamp to every log message. This is critical for debugging, as it allows you to correlate events and know exactly when a client disconnected or a message was sent.

    allow_anonymous false

This is a critical security setting. It disables all anonymous access, forcing every client (your microcontroller, your laptop, your Java server) to provide a valid username and password to connect.

    password_file /mosquitto/config/password.txt

This directive tells the broker where to find the "guest list" of valid users. It points to an absolute path inside the container. This path (/mosquitto/config/password.txt) is made available to the container by the volumes mapping you defined in your docker-compose.yml file.

    memory_limit 250M

This sets a hard limit on the amount of RAM the Mosquitto process can use. 250MB is more than sufficient for a broker with a reasonable number of clients and moderate message throughput.

    persistence false

This is a key decision for your architecture. It tells the broker not to save any in-flight messages (like retained messages or queued data) to disk. If the broker restarts, all data that hasn't been delivered is gone. This is the most efficient, low-resource option, and it's perfect for your use case where the Java server processes data immediately and data loss is acceptable.

    listener 1883 0.0.0.0

This directive tells the broker to open a "listening post" for incoming client connections.
1883 is the port number it will listen on (the standard for MQTT).
0.0.0.0 is the IP address it will listen on. This special address means "listen on all available network interfaces."