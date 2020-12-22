{
  "targets": [
    {
      "target_name": "homegear",
      "sources": [ "homegear.cpp", "HomegearObject.cpp", "IpcClient.cpp", "NapiVariableConverter.cpp" ],
      "libraries": [ "-lhomegear-ipc" ]
    }
  ]
}
