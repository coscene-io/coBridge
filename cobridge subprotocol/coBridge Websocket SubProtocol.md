# coBridge Websocket SubProtocol

## SubProtocol name
    coBridge.websocket.v1


* **login**
    
  当websocket建立连接后，客户端向服务端发送的第一条请求。该请求调用成功之后，才会从服务端获取对应的serverInfo、channels等信息。

  | Fileds      | Type   | Description | e.g.                                                   |
  |-------------|--------|-------------|--------------------------------------------------------|
  | op          | string | 请求类型        | "op": "login"                                          |
  | userId      | string | 当前请求登录的用户id | "userId": "users/08628df5-f156-491d-8779-00bb1db6aa5d" |
  | displayName | string | 当前请求登录的用户名称 | "userName":  "fei.gao"                                 |

  ```JSON
  {
    "op": "login",
    "userId": "users/08628df5-f156-491d-8779-00bb1db6aa5d",
    "userName": "高飞"
  }
  ```

* **subscribe**
  
  请求服务器开始将给定主题（或多个主题）的信息流发送到客户端。对于每一个 channel，一个客户端只能订阅一次（Unsubscribe后可以重新进行Subscribe）。
  
  | Fileds        | Type   | Description                                                                                                   | e.g.                                                                                        |
  |---------------|--------|---------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------|
  | op            | string | 请求类型                                                                                                          | "op": "subscribe"                                                                           |
  | subscriptions | array  | 订阅信息                                                                                                          | "subscriptions": [<br/>{ "id": 0, "channelId": 3 }, <br/>{ "id": 1, "channelId": 5 }<br/>]  |
  | id            | int    | 客户端选择的编号，由客户端自行管理 。<br/>客户端不得在多个活跃的订阅中使用重复的 id。 <br/>服务器会忽略试图重复使用 id 的订阅（并发送错误状态消息）。<br/>取消订阅后，客户端可以重新使用该 ID。 |
  | channelId     | int    | 需要订阅的channel id，与下文中的 advertise 信息相对应                                                                         |
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

  请求服务器停止流式传输客户端之前订阅的信息。

  | Fileds          | Type   | Description             | e.g.                       |
  |-----------------|--------|-------------------------|----------------------------|
  | op              | string | 请求类型                    | "op": "unsubscribe"        |
  | subscriptionIds | array  | 取消订阅的 id 列表，对应上一条订阅信息   | "subscriptionIds": [0, 1]  |
  
  ```JSON
  {
    "op": "unsubscribe",
    "subscriptionIds": [0, 1]
  }
  ```
  
* **advertise**

  通知服务器可用的客户端 channel。只有当服务器先前声明它具有 clientPublish 功能时，客户端才可以向channel发布信息。

  | Fileds     | Type                  | Description                                                            | e.g.                                                                                                                      |
  |------------|-----------------------|------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
  | op         | string                | 请求类型                                                                   | "op": "advertise"                                                                                                         |
  | channels   | array                 | 订阅的 id 列表                                                              | "channels": [{<br/>"id": 2,<br/>"topic": "/tf",<br/>"encoding": "cdr",<br/>"schemaName": "tf2_msgs/msg/TFMessage"<br/>}]  |
  | id         | int                   | 由客户端定义的号码<br/>客户端可以重复使用被unadvertised的ID                                |
  | topic      | string                | topic 名称                                                               |
  | encoding   | string                | 服务器支持的信息编码之一，源自 serverInfo (serverInfo是客户端login后，server主动向client发送的信息) |
  | schemaName | string<br/>(Optional) | topic 数据类型名                                                            |

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

  通知服务器客户端channel不再可用，只有当服务器先前声明它具有 clientPublish 功能时，客户端才可以向channel发布信息。

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
  
  向服务器发送包含原始消息payload的二进制websocket消息。

  只有当服务器先前声明它具有 clientPublish 功能时，客户端才可以向channel发布信息。

  | Bytes | Type    | Description      | 
  |-------|---------|------------------|
  | 1     | byte    | 0x01             |
  | 4     | uint32  | channel id       |
  | 剩余字节  | uint8[] | message payloads |

* **getParameters**

  请求一个或多个参数。 仅在服务器先前声明具备 parameters 功能时才支持。

  | Fileds         | Type         | Description                 | e.g.                                                |
  |----------------|--------------|-----------------------------|-----------------------------------------------------|
  | op             | string       | 请求类型                        | "op": "getParameters"                               |
  | parameterNames | string array | 需要获取的参数列表，如果为空，则检索当前设置的所有参数 | "parameterNames": [<br/>"/tf",<br/>"/costmap"<br/>] |
  | id             | request id   |                             | "id": "******"                                      |

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

  设置一个或多个参数。 仅在服务器先前声明具备 parameters 功能时才支持。

  | Fileds     | Type                                                                          | Description            | e.g.                                                 |
  |------------|-------------------------------------------------------------------------------|------------------------|------------------------------------------------------|
  | op         | string                                                                        | 请求类型                   | "op": "setParameters"                                |
  | id         | string                                                                        | request id             | "id": "******"                                       |
  | parameters | array                                                                         | 需要设置的参数数组              | "parameterNames": [<br/>"/tf",<br/>"/costmap"<br/>]  |
  | name       | string                                                                        | 参数名                    |
  | value      | int<br/>Boolean<br/>String<br/>int[]<br/>boolean[]<br/>string[]<br/>undefined | 如果没有 value 字段，则该参数会被移除 |
  | type       | byte_array<br/>float64<br/>float64_array<br/>undefined                        |
  
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

  订阅参数更新。仅在服务器先前声明具有 parametersSubscribe 功能时才受支持。

  多次发送 subscribeParameterUpdates 会追加参数订阅列表，而不是替换它们.

  请注意，参数最多只能订阅一次。因此，此操作将忽略已订阅的参数。

  使用 unsubscribeParameterUpdates 可取消订阅现有参数。

  | Fileds         | Type         | Description                 | e.g.                                                   |
  |----------------|--------------|-----------------------------|--------------------------------------------------------|
  | op             | string       | 请求类型                        | "op": "subscribeParameterUpdates"                      |
  | parameterNames | string array | 需要订阅的参数列表，如果为空，则订阅当前设置的所有参数 | "parameterNames": ["/video_bitrate","/encoded_device"] |

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

  取消订阅参数更新。 仅在服务器先前声明具有 parametersSubscribe 功能时才受支持。

  | Fileds          | Type         | Description                     | e.g.                                                   |
  |-----------------|--------------|---------------------------------|--------------------------------------------------------|
  | op              | string       | 请求类型                            | "op": "unsubscribeParameterUpdates"                    |
  | parameterNames  | string array | 需要取消订阅的参数列表，如果为空，则取消订阅当前设置的所有参数 | "parameterNames": ["/video_bitrate","/encoded_device"] |

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

  请求调用一个已经被服务器发布的ROS服务，只有当服务器先前声明了services 能力时才支持。

  | Bytes           | Type    | Description    | 
  |-----------------|---------|----------------|
  | 1               | opcode  | 0x02           |
  | 4               | uint32  | service id     |
  | 4               | uint32  | call id，用于区分不同 |
  | 4               | uint32  | encoding 字段长度  |
  | Encoding length | char[]  | encoding 字段信息  |
  | Remaining bytes | uint8[] | 请求信息           |

* **subscribeConnectionGraph**

  订阅 connection graph 更新

  | Fileds     | Type   | Description | e.g.                             |
  |------------|--------|-------------|----------------------------------|
  | op         | string | 请求类型        | "op": "subscribeConnectionGraph" |

  ```JSON
  {
    "op": "subscribeConnectionGraph"
  }
  ```

* **unsubscribeConnectionGraph**

  取消订阅 connection graph 更新

  | Fileds     | Type   | Description | e.g.                                |
  |------------|--------|-------------|-------------------------------------|
  | op         | string | 请求类型        | "op": "unsubscribeConnectionGraph"  |

  ```JSON
  {
    "op": "unsubscribeConnectionGraph"
  }
  ```

* **fetchAsset**

  从服务器获取数据。 仅在服务器先前声明具有 assets 功能时才支持。

  | Fileds    | Type   | Description    | e.g.                                  |
  |-----------|--------|----------------|---------------------------------------|
  | op        | string | 请求类型           | "op": "fetchAsset"                    |
  | uri       | string | 用于定位数据的统一资源标识符 | "uri": "package://coscene/robot.urdf" |
  | requestId | int    | Request 统一标识符  | "requestId": 123                      |

  ```JSON
  {
    "op": "fetchAsset",
    "uri": "package://coscene/robot.urdf",
    "requestId": 123
  }
  ```
