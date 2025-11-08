const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const path = require('path');
const mqtt = require('mqtt');

const protocol = 'mqtt'
const host = '192.168.1.104'
const port = '1883'
const clientId = `mqtt_${Math.random().toString(16).slice(3)}`
let topic="keyboard/arrow"

const connectUrl = `${protocol}://${host}:${port}`

const client = mqtt.connect(connectUrl, {
  clientId,
  clean: true,
  connectTimeout: 4000,
  reconnectPeriod: 1000,
})

let MQTT_CONNECTED = false;

client.on('connect', () => {
  console.log('Connected to MQTT Broker');
  MQTT_CONNECTED = true;
})

const app = express();
const server = http.createServer(app);
const io = socketIo(server);

app.use(express.static('public'));
app.set('view engine', 'ejs');
app.set('views', path.join(__dirname, 'views'));

let controller = null;
const waitingQueue = [];
const clickTimestamps = new Map();
const RATE_LIMIT_MS = 1000;

const logWaiting = () => {
  console.log('Current controller:', controller || 'None');
  console.log('Waiting queue:', waitingQueue.length > 0 ? waitingQueue : 'Empty');
};

io.on('connection', (socket) => {
  console.log('New connection:', socket.id);
  logWaiting();

  socket.emit('your-id', socket.id);

  const grantControl = () => {
    if (MQTT_CONNECTED && !controller && waitingQueue.length > 0) {
      controller = waitingQueue.shift();
      const ctrlSocket = io.sockets.sockets.get(controller);
      if (ctrlSocket) {
        ctrlSocket.emit('control-granted');
        console.log('Control granted to:', controller);
        logWaiting();
        return true;
      } else {
        return grantControl();
      }
    }
    return false;
  };

  const denyControl = () => {
    socket.emit('control-denied');
  };

  socket.on('request-control', () => {
    if (socket.id === controller) {
      socket.emit('control-granted');
      return;
    }

    const index = waitingQueue.indexOf(socket.id);
    if (index !== -1) waitingQueue.splice(index, 1);

    waitingQueue.push(socket.id);
    denyControl();
    console.log('Added to queue:', socket.id);
    logWaiting();
  });

  if (!controller) {
    controller = socket.id;
    socket.emit('control-granted');
    console.log('First controller:', socket.id);
  } else {
    waitingQueue.push(socket.id);
    denyControl();
  }
  logWaiting();

    socket.on('toggle-motor', () => {
        if (socket.id !== controller) {
        console.log('Unauthorized click from:', socket.id);
        socket.emit('cheater-detected');
        return;
        }
        console.log('Toggle Motor clicked by:', socket.id);
        client.publish(topic, 'MOTOR toggle', { qos: 0, retain: false }, (error) => {
            if (error) {
            console.error(error)
            }
        })
    });
    socket.on('rudder-left', () => {
        if (socket.id !== controller) {
        console.log('Unauthorized click from:', socket.id);
        socket.emit('cheater-detected');
        return;
        }
        console.log('Rudder Left clicked by:', socket.id);
        client.publish(topic, 'RUDDER rot -30', { qos: 0, retain: false }, (error) => {
            if (error) {
            console.error(error)
            }
        })
    });
    socket.on('rudder-right', () => {
        if (socket.id !== controller) {
        console.log('Unauthorized click from:', socket.id);
        socket.emit('cheater-detected');
        return;
        }
        console.log('Rudder Right clicked by:', socket.id);
        client.publish(topic, 'RUDDER rot 30', { qos: 0, retain: false }, (error) => {
            if (error) {
            console.error(error)
            }
        })
    });

    socket.on('joystick-move', (data) => {
        if (socket.id !== controller) {
        console.log('Unauthorized joystick from:', socket.id);
        socket.emit('cheater-detected');
        return;
        }
        // data = { x, y, force, angle }
        // only use x, y, and angle
        console.log('Joystick:', socket.id, data);

        let angle = Math.max(-60, Math.min(60, Math.round(data.x)));

        let distance = Math.sqrt(data.x * data.x + data.y * data.y);
        distance = Math.min(1, distance);

        let thrust = -Math.sign(data.y) * distance * 100;

        if (Math.abs(data.y) < 0.1) {
            thrust = distance * 100;
        }

        client.publish(topic, `RUDDER set ${angle}`, { qos: 0, retain: false }, (error) => {
            if (error) {
            console.error(error)
            }
        })
        client.publish(topic, `MOTOR set ${thrust}`, { qos: 0, retain: false }, (error) => {
            if (error) {
            console.error(error)
            }
        })
    });

    socket.on('joystick-end', () => {
        if (socket.id === controller) {
        console.log('Joystick released by:', socket.id);
        }
    });

    const now = Date.now();
    const last = clickTimestamps.get(socket.id) || 0;
    if (now - last < RATE_LIMIT_MS) {
      socket.emit('rate-limited');
      return;
    }
    clickTimestamps.set(socket.id, now);

    console.log('Authorized click! Controller:', socket.id);

  socket.on('disconnect', () => {
    console.log('Disconnect:', socket.id);

    const qIndex = waitingQueue.indexOf(socket.id);
    if (qIndex !== -1) waitingQueue.splice(qIndex, 1);

    if (socket.id === controller) {
      controller = null;
      clickTimestamps.delete(socket.id);
      console.log('Control released by:', socket.id);
      grantControl();
      io.emit('control-released');
    }

    logWaiting();
  });
});

app.get('/', (req, res) => {
  res.render('index');
});

const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
  console.log(`Server running on http://localhost:${PORT}`);
});