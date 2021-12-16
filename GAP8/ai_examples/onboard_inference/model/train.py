#!/usr/bin/env python3
# PYTHON_ARGCOMPLETE_OK

# Copyright (C) 2019 GreenWaves Technologies
# All rights reserved.

# This software may be modified and distributed under the terms
# of the BSD license.  See the LICENSE file for details.

'''Trains a simple convnet on the MNIST dataset.
Gets to 99.25% test accuracy after 12 epochs
(there is still a lot of margin for parameter tuning).
16 seconds per epoch on a GRID K520 GPU.
'''

from __future__ import print_function

import argparse

import argcomplete
import keras
from keras import backend as K
from keras.datasets import mnist
from keras.layers import Activation, Conv2D, Dense, Flatten, MaxPooling2D, BatchNormalization
from keras.models import Sequential

import numpy as np
from PIL import Image
import matplotlib.pyplot as plt
import os

def create_parser():
    # create the top-level parser
    parser = argparse.ArgumentParser(prog='train')

    parser.add_argument('h5_file',
                        default="output.h5",
                        nargs=argparse.OPTIONAL,
                        help='Output - Trained model in h5 format')
    parser.add_argument('-b', '--batch_size',
                        type=int,
                        default=128,
                        help='training batch size')
    parser.add_argument('-e', '--epochs',
                        type=int,
                        default=3,
                        help='training epochs')
    parser.add_argument('-B', '--batch_norm',
                        action='store_true',
                        help='carry out batch normalization')
    return parser

def train(args):
    batch_size = args.batch_size
    num_classes = 12
    epochs = args.epochs

    # input image dimensions
    img_rows, img_cols = 28, 28
    img_color = 3

    # the data, split between train and test sets
    x_train, y_train = np.load("./train.npz")['image'], np.load("./train.npz")['label']

    x_test, y_test = np.load("./val.npz")['image'], np.load("./val.npz")['label']

    plt.figure(figsize=(10,10))
    for i in range(25):
        plt.subplot(5,5,i+1)
        plt.imshow(x_train[i])
        plt.xlabel(y_train[i])
        plt.xticks([])
        plt.yticks([])
        plt.grid(False)
    plt.show()


    if K.image_data_format() == 'channels_first':
        x_train = x_train.reshape(x_train.shape[0], img_color, img_rows, img_cols)
        x_test = x_test.reshape(x_test.shape[0], img_color, img_rows, img_cols)
        input_shape = (img_color, img_rows, img_cols)
    else:
        x_train = x_train.reshape(x_train.shape[0], img_rows, img_cols, img_color)
        x_test = x_test.reshape(x_test.shape[0], img_rows, img_cols, img_color)
        input_shape = (img_rows, img_cols, img_color)

    x_train = x_train.astype('float32')
    x_test = x_test.astype('float32')

    x_train = (x_train / 128) - 1
    x_test = (x_test / 128) - 1

    print('x_train shape:', x_train.shape)
    print(x_train.shape[0], 'train samples')
    print(x_test.shape[0], 'test samples')

    # convert class vectors to binary class matrices
    y_train = keras.utils.to_categorical(y_train, num_classes)
    y_test = keras.utils.to_categorical(y_test, num_classes)

    model = Sequential()
    model.add(Conv2D(32, kernel_size=(3, 3), strides=(2, 2), input_shape=input_shape))
    model.add(Activation('relu'))
    model.add(MaxPooling2D(pool_size=(2, 2)))
    model.add(Conv2D(64, (3, 3), strides=(1, 1)))
    if args.batch_norm:
        model.add(BatchNormalization())
    model.add(Activation('relu'))
    model.add(MaxPooling2D(pool_size=(2, 2)))
    model.add(Flatten())
    model.add(Dense(num_classes))
    if args.batch_norm:
        model.add(BatchNormalization())
    model.add(Activation('softmax'))

    model.compile(loss=keras.losses.categorical_crossentropy,
                  optimizer=keras.optimizers.Adadelta(),
                  metrics=['accuracy'])

    model.fit(x_train, y_train,
              batch_size=batch_size,
              epochs=epochs,
              verbose=1,
              validation_data=(x_test, y_test))
    score = model.evaluate(x_test, y_test, verbose=0)
    print('Test loss:', score[0])
    print('Test accuracy:', score[1])
    model.save(args.h5_file)

def main():
    parser = create_parser()
    argcomplete.autocomplete(parser)
    args = parser.parse_args()
    train(args)

if __name__ == '__main__':
    main()
