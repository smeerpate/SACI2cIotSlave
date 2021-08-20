# SACI2cIotSlave
Makes the raspberry pi act as an iot slave for SAC dispensers

# Compilation
Compile with:
    sudo make

# Deployment
-Copy SACIot.service to /etc/systemd/system/SACIot.service
    sudo cp SACIot.service /etc/systemd/system/SACIot.service

-Enable the service
    sudo systemctl enable SACIot.service

-Start the service
    sudo systemctl start SACIot.service

-Check the status on the service
    sudo systemctl status SACIot.service

-Get complete status log with
    journalctl -u SACIot.service
    (if you want to folow the application's output in real time do "journalctl -u SACIot.service -f")

