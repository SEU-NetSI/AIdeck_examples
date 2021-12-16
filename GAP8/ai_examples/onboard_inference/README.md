# 构建数据集

- 数据集组织方式：

```
\data
   \train
       \class1
          \xxx.jpg
          \xxx.jpg
       \class2
       ...
       \classn
   \test
       \xxx.jpg
       \xxx.jpg
       ...
       \xxx.jpg
```

- data_dir = "训练集根目录"

- 构建方式

```
python dataloader.py
```

保存生成的train.npz、val.npz文件


# 环境要求

- GAP_SDK: release-v3.8.1  (docker or ubuntu)
- Keras: 2.3.1
- tensorflow: 1.15.2
- numpy: 1.16.2


# 运行实例

- 在GAP8上运行

```
make clean all run'
```

- 在gvsoc虚拟平台运行

```
make clean all run platform=gvsoc
```

- 固化到芯片

```
make clean all image flash
```

# 清理编译产物

```
make clean clean_train clean_images clean_model
```