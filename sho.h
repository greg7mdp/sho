#ifndef sho__h__
#define sho__h__

#include <cstdint>
#include <cassert>
#include <algorithm>
#include <utility>

// ---------------------------------------------------------------------
//            S M A L L    H A S H     O P T I M I Z A T I O N
//            ------------------------------------------------
// 
// When you have a lot of very small hash maps with just zero to a couple 
// entries => store a few element in a small array of possible, else 
//            allocate a full hash_map.
//
// Assumptions:
//     - we assume that the value of a pointer is always greater then N.
// ---------------------------------------------------------------------

// ------------------------------------------------------------------------
// ------------------------------------------------------------------------
template <size_t N,
          template <class, class, class, class, class> class RefMap,
          class K,
          class V, 
          class Hash  = std::hash<K>, 
          class Pred  = std::equal_to<K>, 
          class Alloc = std::allocator<std::pair<const K, V> > >
class sho
{
public:
    typedef RefMap<K, V, Hash, Pred, Alloc>     Map;
    typedef typename Map::key_type              key_type;
    typedef typename Map::mapped_type           mapped_type;
    typedef typename Map::value_type            value_type;
    typedef typename Map::hasher                hasher;
    typedef typename Map::key_equal             key_equal;
    typedef typename Map::allocator_type        allocator_type;
    typedef std::pair<K, V>                     mutable_value_type;

    typedef typename Map::size_type             size_type;
    typedef typename Map::difference_type       difference_type;

    typedef typename Map::iterator              map_iterator;

    // ----------------
    template <class Val>
    class Iterator
    {
    public:
        typedef Val value_type;
        typedef typename sho::Map::iterator map_iter;

        explicit Iterator(const value_type *p) : _p((value_type *)p) {}
        explicit Iterator(const map_iter &o) : _p(0), _it(o) {}
        Iterator(const Iterator &o) : _p(o._p), _it(o._it) {}
        Iterator& operator++() { if (_p) ++_p; else ++_it; }
        Iterator& operator--() { if (_p) --_p; else --_it; }
        bool operator==(const Iterator &o) const { return _p ? _p == o._p : _it == o._it; }
        bool operator!=(const Iterator &o) const { return !(*this == o); }
        value_type& operator*() const  { return _p ? *_p : *_it; }
        value_type* operator->() const { return &(operator*()); }

    private:
        friend class sho;
        
        value_type *_p;
        map_iter    _it;
    };

    // ----------------
    typedef Iterator<value_type>        iterator;
    typedef Iterator<const value_type>  const_iterator;

    sho(size_type = 0) : _cnt(0) {}

    ~sho() 
    { 
        if (_hasMap()) 
            _cleanMap();
    }
        
    sho(const sho &o) : _cnt(0) 
    {
        if (o._hasMap())
            _cnt = (uintptr_t)(new Map(o._getMap()));
        else
            _copyLocal(o);
    }

    sho& operator=(const sho &o) 
    {
        if (this == &o)
            return *this;

        if (_hasMap()) 
            _cleanMap();
        if (o._hasMap()) 
            _cnt = (uintptr_t)(new Map(o._getMap()));
        else
            _copyLocal(o);
        return *this;
    }
    
    // Iterator functions
    iterator begin() const             
    {
        return _hasMap() ? iterator(_getMap()->begin()) : iterator(&_items[0]); 
    }
    
    iterator end() const             
    {
        return _hasMap() ? iterator(_getMap()->end()) : iterator(&_items[N]); 
    }

    const_iterator cbegin() const            
    {
        return _hasMap() ? const_iterator(_getMap()->begin()) : const_iterator(&_items[0]); 
    }
    
    const_iterator cend() const             
    {
        return _hasMap() ? const_iterator(_getMap()->end()) : const_iterator(&_items[N]); 
    }
 
    iterator find(const key_type& key)
    {
        if (_hasMap())
            return iterator(_getMap()->find(key));
        else
            for (size_t i=0; i<_cnt; ++i)
                if (key_equal()(_items[i].first, key))
                    return iterator(&_items[i]); 
        return end();
    }

    const_iterator find(const key_type& key) const
    {
        if (_hasMap())
            return const_iterator(_getMap()->find(key));
        else
            for (size_t i=0; i<_cnt; ++i)
                if (key_equal()(_items[i].first, key))
                    return const_iterator(&_items[i]); 
        return cend();
    }

    mapped_type& operator[](const key_type& key)
    {
        if (_hasMap())
            return _getMap()->operator[](key);
        else
        {
            for (size_t i=0; i<_cnt; ++i)
                if (key_equal()(_items[i].first, key))
                    return _items[i].second;
        }
        insert(value_type(key, mapped_type()));
        return this->operator[](key);
    }


    std::pair<iterator, bool> insert(const value_type& val)
    {
        if (_hasMap())
        {
            std::pair<map_iterator, bool> res = _getMap()->insert(val);
            return std::make_pair(iterator(res.first), res.second);
        }

        for (size_t i=0; i<_cnt; ++i)
            if (key_equal()(_items[i].first, val.first))
                return std::make_pair(iterator(&_items[i]), false);

        // not present ... insert it
        if (_cnt == N)
        {
            _switchToMap();
            std::pair<map_iterator, bool> res = _getMap()->insert(val);
            assert(res.second);
            return std::make_pair(iterator(res.first), true);
        }
        *((K *)(&_items[_cnt].first)) = val.first;
        _items[_cnt].second = val.second;
        return std::make_pair(iterator(&_items[_cnt++]), true);
    }

    size_type size() const { return _hasMap() ? _getMap()->size() : _cnt; }

    void clear() 
    {
        if (_hasMap())
            _cleanMap();
        else
        {
            allocator_type alloc;
            for (size_t i=0; i<_cnt; ++i)
                alloc.destruct(_items[i]);
            _cnt = 0;
        }
    }
        
private:
    bool   _hasMap() const { return _cnt > N; }
    
    Map *_getMap() const { assert(_hasMap()); return (Map *)_cnt; }

    void   _cleanMap() { delete _getMap(); _cnt = 0; }

    void   _copyLocal(const sho &o) 
    {
        assert(!_hasMap());
        _cnt = o._cnt;
        for (size_t i=0; i<_cnt; ++i)
            _items[i] = o._items[i];
    }

    void   _createMap()
    { 
        assert(!_hasMap()); 
        _cnt = (uintptr_t)(new Map);
    }

    void   _switchToMap()
    {
        assert(!_hasMap());

        uintptr_t cnt = _cnt;
        _cnt = 0;
        _createMap();

        Map *map = _getMap();
        for (size_t i=0; i<cnt; ++i)
            map->insert(_items[i]);
    }
 
    uintptr_t  _cnt;
    value_type _items[N];
};


#endif // sho__h__
