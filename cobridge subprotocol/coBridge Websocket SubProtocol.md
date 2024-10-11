# coBridge Websocket SubProtocol

## SubProtocol name
  coBridge.websocket.v1

## Messages

* **login**
    
  login is the first message after websocket connected, 
  server will send `serverInfo`,`channels` and other messages to client after received `login` from client.

  | Fileds      | Type   | Description | e.g.                                                   |
  |-------------|--------|-------------|--------------------------------------------------------|
  | op          | string |             | "op": "login"                                          |
  | userId      | string |             | "userId": "users/08628df5-f156-491d-8779-00bb1db6aa5d" |
  | displayName | string |             | "userName":  "fei.gao"                                 |

  ```JSON
  {
    "op": "login",
    "userId": "users/08628df5-f156-491d-8779-00bb1db6aa5d",
    "userName": "高飞"
  }
  ```

* **subscribe**

  Requests that the server start streaming messages on a given topic (or topics) to the client.
  A client may only have one subscription for each channel at a time.

  | Fileds        | Type   | Description                                                                                                                                                                                                                                                          | e.g.                                                                                        |
  |---------------|--------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------|
  | op            | string |                                                                                                                                                                                                                                                                      | "op": "subscribe"                                                                           |
  | subscriptions | array  |                                                                                                                                                                                                                                                                      | "subscriptions": [<br/>{ "id": 0, "channelId": 3 }, <br/>{ "id": 1, "channelId": 5 }<br/>]  |
  | id            | int    | number chosen by the client. <br/>The client may not reuse ids across multiple active subscriptions. <br/>The server may ignore subscriptions that attempt to reuse an id (and send an error status message). <br/>After unsubscribing, the client may reuse the id. |
  | channelId     | int    | number, corresponding to previous Advertise message(s)                                                                                                                                                                                                               |
  ```JSON
  {
    "op": "subscribe",
    "subscriptions": [
      { "id": 0, "channelId": 3 },
      { "id": 1, "channelId": 5 }
    ]
  }
  ```

* **unsubscribe**

  Requests that the server stop streaming messages to which the client previously subscribed.

  | Fileds          | Type   | Description                                                     | e.g.                       |
  |-----------------|--------|-----------------------------------------------------------------|----------------------------|
  | op              | string |                                                                 | "op": "unsubscribe"        |
  | subscriptionIds | array  | array of number, corresponding to previous Subscribe message(s) | "subscriptionIds": [0, 1]  |
  
  ```JSON
  {
    "op": "unsubscribe",
    "subscriptionIds": [0, 1]
  }
  ```
  
* **advertise**

  Informs the server about available client channels. Note that the client is only allowed to advertise channels if the server previously declared that it has the clientPublish capability.

  | Fileds     | Type                  | Description                                                                                   | e.g.                                                                                                                      |
  |------------|-----------------------|-----------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
  | op         | string                |                                                                                               | "op": "advertise"                                                                                                         |
  | channels   | array                 |                                                                                               | "channels": [{<br/>"id": 2,<br/>"topic": "/tf",<br/>"encoding": "cdr",<br/>"schemaName": "tf2_msgs/msg/TFMessage"<br/>}]  |
  | id         | int                   | number chosen by the client. The client may reuse ids that have previously been unadvertised. |
  | topic      | string                | topic name                                                                                    |
  | encoding   | string                | one of the message encodings, supported by the server, from serverInfo                        |
  | schemaName | string<br/>(Optional) | name of message type                                                                          |

  ```JSON
  {
    "op": "advertise",
    "channels": [
      {
        "id": 2,
        "topic": "/tf",
        "encoding": "cdr",
        "schemaName": "tf2_msgs/msg/TFMessage"
      }
    ]
  }
  ```
  
* **unadvertise**

  Informs the server that client channels are no longer available. Note that the client is only allowed to unadvertise channels if the server previously declared that it has the `clientPublish` capability.

  | Fileds     | Type   | Description | e.g.                  |
  |------------|--------|-------------|-----------------------|
  | op         | string | 请求类型        | "op": "unadvertise"   |
  | channelIds | array  |             | "channelIds": [1, 2]  |

  ```JSON
  {
    "op": "unadvertise",
    "channelIds": [1, 2]
  }
  ```

* **client message data**

  Sends a binary websocket message containing the raw messsage payload to the server.

  Note that the client is only allowed to publish messages if the server previously declared that it has the `clientPublish` capability

  | Bytes            | Type    | Description      | 
  |------------------|---------|------------------|
  | 1                | byte    | 0x01             |
  | 4                | uint32  | channel id       |
  | remaining bytes  | uint8[] | message payloads |

* **getParameters**

  Request one or more parameters. Only supported if the server previously declared that it has the `parameters` capability.

  | Fileds         | Type         | Description                                                                        | e.g.                                                |
  |----------------|--------------|------------------------------------------------------------------------------------|-----------------------------------------------------|
  | op             | string       |                                                                                    | "op": "getParameters"                               |
  | parameterNames | string array | param list that you needs, leave empty to retrieve all currently set parameters    | "parameterNames": [<br/>"/tf",<br/>"/costmap"<br/>] |
  | id             | request id   | undefined, arbitrary string used for identifying the corresponding server response | "id": "******"                                      |

  ```JSON
  {
    "op": "getParameters",
    "parameterNames": [
      "/video_bitrate",
      "/encoded_device"
    ],
    "id": "******"
  }
  ```

* **setParameters**

  Set one or more parameters. Only supported if the server previously declared that it has the `parameters` capability.

  | Fileds     | Type                                                                          | Description                                                                                                                                                                                                                                                                        | e.g.                                                 |
  |------------|-------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------|
  | op         | string                                                                        |                                                                                                                                                                                                                                                                                    | "op": "setParameters"                                |
  | id         | string or undefined                                                           | arbitrary string used for identifying the corresponding server response. If this field is not set, the server may not send a response to the client.                                                                                                                               | "id": "******"                                       |
  | parameters | array                                                                         |                                                                                                                                                                                                                                                                                    | "parameterNames": [<br/>"/tf",<br/>"/costmap"<br/>]  |
  | name       | string                                                                        | parameter name                                                                                                                                                                                                                                                                     |
  | value      | int<br/>Boolean<br/>String<br/>int[]<br/>boolean[]<br/>string[]<br/>undefined | undefined. If the value is not set (undefined), the parameter shall be unset (removed).                                                                                                                                                                                            |
  | type       | byte_array<br/>float64<br/>float64_array<br/>undefined                        | If the type is byte_array, value shall be a base64 encoded string. <br/>If the type is float64, value must be a valid decimal or integer value that can be represented by a float64. <br/>If the type is float64_array, value must be an array of valid decimal or integer values. |
  
  ```JSON
  {
    "op": "setParameters",
    "parameters": [
      {
        "name": "/video_bitrate",
        "value": 800000
      },
      {
        "name": "/encoded_device",
        "value": "WyJ2aWRlXzAiLCAidmlkZW9fMSJd",
        "type": "byte_array"
      }
    ],
    "id": "******"
  }
  ```

* **subscribeParameterUpdates**

  Subscribe to parameter updates. Only supported if the server previously declared that it has the parametersSubscribe capability.

  Sending subscribeParameterUpdates multiple times will append the list of parameter subscriptions, not replace them. 

  Note that parameters can be subscribed at most once. Hence, this operation will ignore parameters that are already subscribed. Use unsubscribeParameterUpdates to unsubscribe from existing parameter subscriptions.

  | Fileds         | Type         | Description                                                                     | e.g.                                                   |
  |----------------|--------------|---------------------------------------------------------------------------------|--------------------------------------------------------|
  | op             | string       |                                                                                 | "op": "subscribeParameterUpdates"                      |
  | parameterNames | string array | list of parameters, leave empty to subscribe to all currently known parameters  | "parameterNames": ["/video_bitrate","/encoded_device"] |

  ```JSON
  {
    "op": "subscribeParameterUpdates",
    "parameterNames": [
      "/video_bitrate",
      "/encoded_device"
    ]
  }
  ```

* **unsubscribeParameterUpdates**

  Unsubscribe from parameter updates. 
 
  Only supported if the server previously declared that it has the `parametersSubscribe` capability.

  | Fileds          | Type         | Description                                                               | e.g.                                                   |
  |-----------------|--------------|---------------------------------------------------------------------------|--------------------------------------------------------|
  | op              | string       |                                                                           | "op": "unsubscribeParameterUpdates"                    |
  | parameterNames  | string array | list of parameters, leave empty to unsubscribe from all parameter updates | "parameterNames": ["/video_bitrate","/encoded_device"] |

  ```JSON
  {
    "op": "unsubscribeParameterUpdates",
    "parameterNames": [
      "/video_bitrate",
      "/encoded_device"
    ]
  }
  ```

* **service call request**

  Request to call a service that has been advertised by the server.
  
  Only supported if the server previously declared the `services` capability.

  | Bytes           | Type    | Description                                                             | 
  |-----------------|---------|-------------------------------------------------------------------------|
  | 1               | opcode  | 0x02                                                                    |
  | 4               | uint32  | service id                                                              |
  | 4               | uint32  | call id， a unique number to identify the corresponding service response |
  | 4               | uint32  | encoding length                                                         |
  | Encoding length | char[]  | encoding, one of the encodings supported by the server                  |
  | Remaining bytes | uint8[] | request payload                                                         |

* **subscribeConnectionGraph**

  Subscribe to connection graph updates. 

  Only supported if the server previously declared that it has the `connectionGraph` capability.

  | Fileds     | Type   | Description | e.g.                             |
  |------------|--------|-------------|----------------------------------|
  | op         | string |             | "op": "subscribeConnectionGraph" |

  ```JSON
  {
    "op": "subscribeConnectionGraph"
  }
  ```

* **unsubscribeConnectionGraph**

  Unsubscribe from connection graph updates. 

  Only supported if the server previously declared that it has the `connectionGraph` capability.

  | Fileds     | Type   | Description | e.g.                                |
  |------------|--------|-------------|-------------------------------------|
  | op         | string |             | "op": "unsubscribeConnectionGraph"  |

  ```JSON
  {
    "op": "unsubscribeConnectionGraph"
  }
  ```

* **fetchAsset**

  Fetch an asset from the server. 

  Only supported if the server previously declared that it has the `assets` capability.

  | Fileds    | Type   | Description                                                            | e.g.                                  |
  |-----------|--------|------------------------------------------------------------------------|---------------------------------------|
  | op        | string |                                                                        | "op": "fetchAsset"                    |
  | uri       | string | uniform resource identifier to locate a single asset                   | "uri": "package://coscene/robot.urdf" |
  | requestId | int    | unique 32-bit unsigned integer which is to be included in the response | "requestId": 123                      |

  ```JSON
  {
    "op": "fetchAsset",
    "uri": "package://coscene/robot.urdf",
    "requestId": 123
  }
  ```
