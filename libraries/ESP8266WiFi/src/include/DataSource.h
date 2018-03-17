/* DataSource.h - a read-only object similar to Stream, but with less methods
 * Copyright (c) 2016 Ivan Grokhotkov. All rights reserved.
 * This file is distributed under MIT license.
 */
#ifndef DATASOURCE_H
#define DATASOURCE_H

#include <assert.h>

class DataSource {
public:
    virtual ~DataSource() {}
    virtual size_t available() = 0;
    virtual const uint8_t* get_buffer(size_t size) = 0;
    virtual void release_buffer(const uint8_t* buffer, size_t size) = 0;

};

class BufferDataSource : public DataSource {
public:
    BufferDataSource(const uint8_t* data, size_t size) :
        _data(data),
        _size(size)
    {
    }

    size_t available() override
    {
        return _size - _pos;
    }

    const uint8_t* get_buffer(size_t size) override
    {
        (void)size;
        assert(_pos + size <= _size);
        return _data + _pos;
    }

    void release_buffer(const uint8_t* buffer, size_t size) override
    {
        (void)buffer;
        assert(buffer == _data + _pos);
        _pos += size;
    }

protected:
    const uint8_t* _data;
    const size_t _size;
    size_t _pos = 0;
};

template<typename TStream>
class BufferedStreamDataSource : public DataSource {
public:
    BufferedStreamDataSource(TStream& stream, size_t size) :
        _stream(stream),
        _size(size)
    {
    }

    size_t available() override
    {
        return _size - _pos;
    }

    const uint8_t* get_buffer(size_t size) override
    {
        assert(_pos + size <= _size);

        //Data that was already read from the stream but not released (e.g. if error occured). Otherwise this should be 0.
        const size_t stream_read = _streamPos - _pos;

        if (_bufferSize < size) {
            uint8_t *new_buffer = new uint8_t[size];
            //If stream reading is ahead, than some data is already in the old buffer and needs to be copied to new resized buffer
            if (_buffer && stream_read > 0) {
                memcpy(new_buffer, _buffer.get(), stream_read);
            }
            _buffer.reset(new_buffer);
            _bufferSize = size;
        }

        //If error in tcp_write in ClientContext::_write_some() occured earlier and therefore release_buffer was not called, than there might not even be data needed to be read from the stream
        if (size > stream_read) {
            //Remaining bytes to read from stream
            const size_t stream_rem = size - stream_read;
            const size_t cb = _stream.readBytes(reinterpret_cast<char*>(_buffer.get() + stream_read), stream_rem);
            assert(cb == stream_rem);
            (void)cb;
            _streamPos += stream_rem;
        }
        return _buffer.get();

    }

    void release_buffer(const uint8_t* buffer, size_t size) override
    {
        (void)buffer;
        _pos += size;
        //Release size needs to be the same as get_buffer(size)
        assert(_pos == _streamPos);
    }

protected:
    TStream & _stream;
    std::unique_ptr<uint8_t[]> _buffer;
    size_t _size;
    size_t _pos = 0;
    size_t _bufferSize = 0;
    size_t _streamPos = 0;
};

class ProgmemStream
{
public:
    ProgmemStream(PGM_P buf, size_t size) :
        _buf(buf),
        _left(size)
    {
    }

    size_t readBytes(char* dst, size_t size)
    {
        size_t will_read = (_left < size) ? _left : size;
        memcpy_P((void*)dst, (PGM_VOID_P)_buf, will_read);
        _left -= will_read;
        _buf += will_read;
        return will_read;
    }

protected:
    PGM_P _buf;
    size_t _left;
};


#endif //DATASOURCE_H
