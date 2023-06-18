const mqtt = require('mqtt');
const mongoose = require('mongoose');
const moment = require('moment');
const shortId = require('shortid');
const Events = require('./eventModel');

const topic = "nhatquang3102/A";

  const client = mqtt.connect('mqtt://ngoinhaiot.com:1111', { 
    username: 'nhatquang3102',
    password: '4CFDA840F1F84281'
  });


// MongoDB Connection Success
mongoose.connection.on('connected', async () => {
    console.log('MongoDb connected');
});

// MongoDB Connection Fail
mongoose.connection.on('error', async (err) => {
    console.log('Error connecting MongoDb', err);
});


client.on('connect', async () => {
    await mongoose.connect('mongodb+srv://nhatquang3102:nhatquang310@iot.4crfx97.mongodb.net/?retryWrites=true&w=majority');

    console.log('MQTT Connected');
    client.subscribe(topic);

})

client.on('message', async (topic, message) => {
    console.log('MQTT received Topic:', topic.toString() ,', Message: ', message.toString());

    let data = message.toString();
    data = JSON.parse(data);
    data.created = moment().utc().add(5, 'hours');
    data._id = shortId.generate();
    // Save live data into database
    await saveData(data);

}) 

saveData = async (data) => {
   data = new Events(data);
    data = await data.save();
    console.log('Saved data:', data);


}