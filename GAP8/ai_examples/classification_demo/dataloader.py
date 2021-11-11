import os
import numpy as np
from PIL import Image
import random


def savelist(data_dir):
	"""
	从输入根目录获取图像路径、标签列表
	"""
	image_paths = []
	image_labels = []

	print(data_dir)
	
	image_classes = os.listdir(data_dir)
	print(image_classes)

	for image_class in image_classes:
	    sub_dir = os.path.join(data_dir + "\\" + image_class)
	    # 进入子类目录
	    image_names = os.listdir(sub_dir)
	    for image_name in image_names:
	        sub_path = os.path.join(sub_dir + "\\" + image_name)
	        # 操作每类图片
	        image_paths.append(sub_path)
	        image_labels.append(image_classes.index(sub_dir[38:]))


	# 随机打乱
	random.seed(123)
	random.shuffle(image_paths)
	random.seed(123)
	random.shuffle(image_labels)

	image_count = len(image_paths)
	split = 0.2

	val_image_paths = image_paths[:int(split*image_count)]
	train_image_paths = image_paths[int(split*image_count):]
	
	val_image_labels = image_labels[:int(split*image_count)]
	train_image_labels = image_labels[int(split*image_count):]

	return train_image_paths, train_image_labels, val_image_paths, val_image_labels
	



def save_as_numpy(image_paths, image_labels,str):
	d = len(image_paths)
	print(d)
	data = np.empty((d, 28, 28, 3),dtype=np.uint8)
	print(data)
	while d>0:
	    img = Image.open(image_paths[d-1])
	    img = img.resize((28,28),Image.ANTIALIAS)
	    img_ndarray = np.array(img)
	    print(img_ndarray.shape)
	    data[d - 1] = img_ndarray
	    d = d - 1

	print(str + "转换完成")
	print(data)
	
	d = len(image_labels)
	label = np.zeros((d),dtype=np.uint8)
	print(label)
	while d>0:
	    label[d - 1] = image_labels[d-1]
	    d = d - 1
	print(str + "转换完成")
	print(label)

	np.save(str + '_images.npy',data)
	np.save(str + '_labels.npy',label)
	np.savez(str + '.npz',image=data,label=label)
	



if __name__=='__main__':

	data_dir = ""

	train_images, train_labels, val_images, val_labels = savelist(data_dir)
	

	save_as_numpy(train_images, train_labels, str='train')
	save_as_numpy(val_images, val_labels, str='val')

