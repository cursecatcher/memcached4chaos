ps aux | grep Server | awk '{print $2" "$12}'
echo "Enter pid"
read PID
kill -9 ${PID}

