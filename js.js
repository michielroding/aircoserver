var wsServer   = 'ws://aircoserver.local:81';
var lastAirflow  = null;
var airflowIndex = 1;
var airflowChange;

var lastFanSpeed  = null;
var fanSpeedChange;
var fanSpeedIndex = 1;

var degreesElement    = document.querySelector('#degrees');
var stateElement      = document.querySelector('#state > i');
var airflowContainer  = document.querySelector('#airflow');
var fanspeedContainer = document.querySelector('#fan-speed');

var aircoWs     = new WebSocket(wsServer, "airco");
aircoWs.onmessage = function (data) {
    data = JSON.parse(data.data);

    degreesElement.value = data.temperature;

    setMode(data.mode);
    setState(data.state);
    setFanSpeed(data.fan);
    setAirflow(data.airflow);

};
aircoWs.onclose   = function () {
    console.warn('WebSocket closed');
};
aircoWs.onerror   = function (event) {
    console.error('WebSocket error observed:', event);
};

degreesElement.addEventListener('blur', publishState);
document.querySelectorAll('#mode > *').forEach(function (element) {
    element.addEventListener('click', function () {
        setMode(this.dataset.mode);
        publishState();
    });
});

degreesElement.addEventListener('change', publishState);

stateElement.addEventListener('click', function () {
    setState(stateElement.classList.contains('active') ? "off" : "on");
    publishState();
});

airflowContainer.addEventListener('click', function () {
    switch (lastAirflow) {
        case "open":
            setAirflow("dir_1");
            break;
        case "dir_1":
            setAirflow("dir_2");
            break;
        case "dir_2":
            setAirflow("dir_3");
            break;
        case "dir_3":
            setAirflow("dir_4");
            break;
        case "dir_4":
            setAirflow("dir_5");
            break;
        case "dir_5":
            setAirflow("dir_6");
            break;
        case "dir_6":
            setAirflow("change");
            break;
        case "change":
            setAirflow("open");
            break;
    }
    publishState();
});

fanspeedContainer.addEventListener('click', function () {
    if (lastFanSpeed === 4) {
        setFanSpeed(1);
    } else {
        setFanSpeed(lastFanSpeed + 1);
    }
    publishState();
});

function setFanSpeed(fan) {
    if (lastFanSpeed !== fan) {
        if (fan === 4) {
            clearInterval(fanSpeedChange);
            addClass('#fan-speed .ring_1', 'active');
            fanSpeedChange = setInterval(
                function () {
                    if (fanSpeedIndex >= 4) {
                        fanSpeedIndex = 1;
                    }
                    removeClass('#fan-speed .ring_2, #fan-speed .ring_3', 'active');

                    for (var i = 2; i <= fanSpeedIndex; i++) {
                        addClass('#fan-speed .ring_' + i, 'active');
                    }

                    fanSpeedIndex++;
                },
                1000
            );
        } else {
            clearInterval(fanSpeedChange);
            removeClass('#fan-speed .ring', 'active');
            for (var i = 0; i <= fan; i++) {
                addClass('#fan-speed .ring_' + i, 'active');
            }
        }
        lastFanSpeed = fan;
    }
}

function setAirflow(airflow) {
    if (lastAirflow !== airflow) {
        if (airflow === "change") {
            clearInterval(airflowChange);
            airflowChange = setInterval(
                function () {
                    removeClass('#airflow .dir.active', 'active');
                    if (airflowIndex >= 7) {
                        airflowIndex = 1;
                    }
                    addClass('#airflow .dir_' + airflowIndex, 'active');
                    airflowIndex++;
                },
                1000
            );
        }
        else if (airflow === "open") {
            clearInterval(airflowChange);
            airflowChange = setInterval(
                function () {
                    if (airflowIndex >= 4) {
                        airflowIndex = 1;
                    }
                    switch (airflowIndex) {
                        case 1:
                            removeClass('#airflow .boost', 'active');
                            removeClass('#airflow .dir', 'active');
                            break;
                        case 2:
                            addClass('#airflow .boost', 'active');
                            break;
                        case 3:
                            addClass('#airflow .dir', 'active');
                            break;
                    }
                    airflowIndex++;
                },
                1000
            );
        }
        else if (airflow.substr(0, 4) === "dir_") {
            clearInterval(airflowChange);
            removeClass('#airflow .dir.active', 'active');
            addClass('#airflow .' + airflow, 'active');
        }
    }
    lastAirflow = airflow;
}

function setMode(mode) {
    removeClass('#mode .active', 'active');
    addClass('#mode > [data-mode="' + mode + '"]', 'active');
}

function setState(state) {
    if (state === "on") {
        stateElement.classList.add('active');
    } else {
        stateElement.classList.remove('active');
    }
}

function publishState() {
    aircoWs.send(
        JSON.stringify(
            {
                'state'      : stateElement.classList.contains('active') ? "on" : "off",
                'mode'       : document.querySelector('#mode .active') ? document.querySelector('#mode .active').dataset.mode : 'auto',
                'temperature': degreesElement.value,
                'fan'        : lastFanSpeed,
                'airflow'    : lastAirflow
            }
        )
    );
}

function removeClass(selector, classname) {
    document.querySelectorAll(selector).forEach(function (element) {
        element.classList.remove(classname);
    });
}

function addClass(selector, classname) {
    document.querySelectorAll(selector).forEach(function (element) {
        element.classList.add(classname);
    });
}