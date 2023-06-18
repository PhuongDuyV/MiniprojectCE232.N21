
const mongoose = require('mongoose');
const moment = require('moment');
const Schema = mongoose.Schema;

const EventsSchema = new Schema({
  /*  device_id: {
        type: String,
        required: true
    },*/
    DA: {
        type: Number,
        required: true
    },
    ND: {
        type: Number,
        required: true
    },

    created: {
        type: Date,
        default: moment().utc().add(5, 'hours')
    }
}, {
        _id: false,
        id: false,
        versionKey: false,
        strict: false
    }
);


module.exports = mongoose.model('Events', EventsSchema);