# inertial-handwriting

Started as project for ECE M119 course at UCLA during Winter 2021. May possibly update for fun.

A "virtual pen" that determines which digit the user is writing based on the motion they make holding the pen, without the use of a camera.
Press the button to record motion.

We aim to make a “virtual pen” that determines what a person is writing based on the motion they make holding the pen, without the use of a camera. The decoded text is sent to another device. We aim to minimize the need for external computation or additional sensors outside of the device. Ideally the user can “write” on any surface or in the air with little restriction on location, orientation, or occlusion between the pen and the receiving device. We want the handwriting recognition to be robust to differences in scale (e.g. from the scale of writing on paper to writing for lecturing purposes), writing speed, and handwriting style.

As a proof of concept, we will implement recognition for a subset of arabic numerals. Our main objective is to make the device capable of recognizing the numbers accurately given robust motions. The device will be able to tell the difference between the hand motions when writing different numbers and analyze the motion data. The user will press a button to indicate when they are writing a single digit as opposed to just moving the pen between digits or are not using the pen.
