module.exports = [
  { 
      "type": "heading", 
      "size": 1,
      "defaultValue": "Star Wars Characters" 
  }, 
  {
      "type": "section",
      "items": [
            {
                "type": "heading",
                "defaultValue": "Display"
            },
            {
                "type": "select",
                "messageKey": "third_line",
                "label": "Third line",
                "description": "If you want to conserve some battery, do not use seconds option.",
                "defaultValue": "n",
                "options": [
                    {
                      "label": "Nothing",
                      "value": "n"
                    }, {
                      "label": "Seconds",
                      "value": "s"
                    }, {
                      "label": "Date",
                      "value": "d"
                    }
                ]
            },
            {
                "type": "toggle",
                "messageKey": "steps",
                "label": "Show steps",
                "defaultValue": true,
                "capabilities": ["HEALTH"]
            },
            {
                "type": "checkboxgroup",
                "messageKey": "characters",
                "label": "Characters",
                "defaultValue": [true, true, true, true, true, true, true, true],
                "options": [
                    "Darth Vader",
                    "Luke Skywalker with green saber",
                    "Han Solo",
                    "Luke Skywalker from episode 1",
                    "C3PO",
                    "Rey",
                    "Finn",
                    "Rey with staff"
                ]
            },
            {
                "type": "checkboxgroup",
                "messageKey": "switch_events",
                "label": "Switch characters event",
                "defaultValue": [true, true, false],
                "options": [
                    "Every minute",
                    "Watch shake/tap",
                    "Custom timer"
                ]
            },
            {
                "type": "slider",
                "messageKey": "switch_timer",
                "label": "Character switch timer",
                "description": "In seconds. Only active, if \"Custom timer\" option is selected above. <strong>Warning:</strong> Setting to low value will consume more battery!",
                "defaultValue": 1,
                "min": 1,
                "max": 59
            }
        ]
    },
    {
        "type": "section",
        "items": [
            {
                "type": "heading",
                "defaultValue": "Vibration"
            },
            {
                "type": "slider",
                "messageKey": "bluetoothvibe",
                "label": "Bluetooth vibration duration",
                "description": "In seconds. If set to 0, there will not be any vibration.",
                "defaultValue": 0,
                "min": 0,
                "max": 1,
                "step": 0.050
            }
        ]
    },
    {
        "type": "submit",
        "defaultValue": "Save"
    }
];
