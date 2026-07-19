#pragma once

#include <ctime>

class peer;

enum wstate3 : u_char
{
    S_RIGHT =  0x00,
    S_LOCKED = 0x10,
    S_LEFT =   0x20,
    S_TOGGLE = 0x40, // @note including mailbox which just toggles the visuals
    S_PUBLIC = 0x80
};

/* locks that only occupy a number of tiles, and not the whole world. */
#define is_tile_lock(id) (id == 202/*small*/ || id == 204/*big*/ || id == 206/*huge*/ || id == 4994/*builder*/)

enum wstate4 : u_char
{
    S_WATER =    0x04,
    S_GLUE =     0x08,
    S_FIRE =     0x10,
    /* paint buckets */
    S_RED =      0x20,
    S_GREEN =    0x40,
    S_YELLOW =   S_RED | S_GREEN,
    S_BLUE =     0x80,
    S_AQUA =     S_GREEN | S_BLUE,
    S_PURPLE =   S_RED | S_BLUE,
    S_CHARCOAL = S_RED | S_GREEN | S_BLUE,
    S_VANISH =   S_RED | S_YELLOW | S_GREEN | S_AQUA | S_BLUE | S_PURPLE | S_CHARCOAL
};

enum lock_state : u_char
{
    DISABLE_MUSIC = 0x10,
    DISABLE_MUSIC_RENDER = 0x20,
    RAINBOWS = 0x80
};

struct block 
{
    block(short _fg = 0, short _bg = 0, 
        std::time_t _tick = 0,
        std::string _label = "", u_char s3 = 0, u_char s4 = 0
    ) : fg(_fg), bg(_bg), tick(_tick), label(_label), state{0, 0, s3, s4} {}
    
    short fg{0}, bg{0};
    
    std::time_t tick{}; // @note unix epoch seconds for tree growth, providers, etc.
    std::string label{}; // @note sign/door label @todo store in seperate class

    u_char state[4]{};

    u_char hits[2] = {0, 0}; // @note fg, bg
};
#define cord(x,y) (y * 100 + x)

struct door 
{
    door(std::string _dest, std::string _id, std::string _password, ::pos _pos) : 
        dest(_dest), id(_id), password(_password), pos(_pos) {}

    std::string dest{};
    std::string id{};
    std::string password{};
    ::pos pos{};
};

struct display
{
    display(u_int _id, ::pos _pos) : id(_id), pos(_pos) {}

    u_int id{};
    ::pos pos{};
};

/* vending machine stock — price >0 = WL per item, price <0 = |price| items per 1 WL */
struct vending
{
    vending(::pos _pos = {}, u_short _id = 0, u_short _count = 0, int _price = 0, u_short _earned = 0) :
        pos(_pos), id(_id), count(_count), price(_price), earned(_earned) {}

    ::pos pos{};
    u_short id{};
    u_short count{};
    int price{};
    u_short earned{}; // @note World Locks waiting for owner withdraw
};

/* Magplant 5000 storage — sucks matching drops when enabled */
struct magplant
{
    magplant(::pos _pos = {}, u_short _id = 0, u_short _count = 0, bool _enabled = false) :
        pos(_pos), id(_id), count(_count), enabled(_enabled) {}

    ::pos pos{};
    u_short id{};    // @note filter item (0 = unset)
    u_short count{};
    bool enabled{};  // @note collecting drops
    static constexpr u_short CAPACITY = 5000;
};

/* chemical combiner (E-Z Cook Oven, Laboratory, ...) — items swallowed while closed */
struct combiner
{
    combiner(::pos _pos = {}) : pos(_pos) {}

    ::pos pos{};
    std::vector<::slot> contents{};
    bool output_ready{}; // @note open oven is holding crafted/returned drops to collect
};

/* Small/Big/Huge/Builder lock — claims a flood-filled area of tiles */
struct tile_lock
{
    tile_lock(::pos _pos = {}, u_short _lock_id = 0, int _owner = 0, bool _is_public = false) :
        pos(_pos), lock_id(_lock_id), owner(_owner), is_public(_is_public) {}

    ::pos pos{};
    u_short lock_id{};
    int owner{};
    std::array<int, 20> access{};
    bool is_public{};
    std::vector<::pos> area{}; // @note claimed tiles (includes lock tile)
};

struct random_block
{
    random_block(u_char _value, ::pos _pos) : value(_value), pos(_pos) {}

    u_char value{};
    ::pos pos{};
};

struct object 
{
    object(u_short _id, u_short _count, ::pos _pos, u_int _uid) : id(_id), count(_count), pos(_pos), uid(_uid) {}
    u_short id{};
    u_short count{};
    ::pos pos{};

    u_int uid{};
};

/* one entry inside a mailbox, bulletin board or donation box tile */
struct letter
{
    letter(int _uid, std::string _from, std::string _message, ::pos _pos, ::slot _im = {0, 0}) : 
        uid(_uid), from(_from), message(_message), pos(_pos), im(_im) {}

    int uid{}; // @note sender's user id
    std::string from{}; // @note sender's growid, shown even when they're offline
    std::string message{};
    ::pos pos{}; // @note which tile this letter belongs to
    ::slot im{0, 0}; // @note donation boxes only: the donated item
};

class world 
{
public:
    world(const std::string& name = "");

    std::string name{};

    int owner{ 00 }; // @note owner of world using peer's user id.
    std::array<int, 20> access{}; // @note {user_id} @credit https://www.growtopiagame.com/forums/member/440629-yeldyt
    bool is_public{}; // @note checks if world is public to break/place
    u_char lock_state{0x00}; // @note uses lock_state::
    u_char minimum_entry_level{1}; // @note minimal level required to enter a world

    u_char visitors{}; // @note the current number of peers in a world, excluding invisable peers
    u_char netid_counter{}; // @note a number that only increases, this value resets during ~world()

    std::vector<::block> blocks; // @note all blocks, size of 1D meaning (6000) instead of 2D (100, 60)
    u_int last_object_uid{0};
    std::vector<::object> objects{};
    std::vector<::door> doors{};
    std::vector<::display> displays{};
    std::vector<::vending> vendings{};
    std::vector<::magplant> magplants{};
    std::vector<::combiner> combiners{};
    std::vector<::tile_lock> tile_locks{};
    std::vector<::random_block> random_blocks{};
    std::vector<::letter> letters{}; // @note mailbox/bulletin/donation tile contents

    ::pos weather{};

    bool dirty{}; // @note needs DB flush
    void mark_dirty() { dirty = true; }
};
extern std::vector<world> worlds;

/* how many tiles a tile lock claims (Small=10, Big=48, Huge/Builder=200) */
extern int tile_lock_capacity(u_short lock_id);

/* flood-fill claim around lock_pos; returns claimed coords including the lock */
extern std::vector<::pos> claim_tile_lock_area(::world &world, ::pos lock_pos, int capacity);

/* tile lock covering this coordinate, or nullptr */
extern ::tile_lock *tile_lock_at(::world &world, ::pos punch);
extern const ::tile_lock *tile_lock_at(const ::world &world, ::pos punch);

/* true if peer may build/break at punch under world + tile lock rules */
extern bool peer_can_edit_tile(const ::peer *pPeer, const ::world &world, ::pos punch);

/* place / remove a tile lock entry and refresh S_LOCKED flags on its area */
extern void apply_tile_lock(::world &world, ::tile_lock &lock);
extern void remove_tile_lock(::world &world, ::pos lock_pos);

/* seconds elapsed since plant/provider tick (offline-safe) */
extern int block_elapsed_seconds(std::time_t tick);

/* plant/reset timestamp pre-aged for server.cfg growth_speed (offline-safe) */
extern std::time_t growth_planted_tick(int grow_seconds);

/* true while at least one Xenonite Crystal stands in the world (grants S_DOUBLE_JUMP) */
extern bool world_has_xenonite(const ::world &world);

extern bool world_save(const ::world &world);
extern bool world_load(::world &world, const std::string &name);
extern void autosave_worlds();
extern void save_all_worlds();

extern void send_action(ENetPeer& p, const std::string& action, const std::string& str);

extern void send_data(ENetPeer &peer, const std::vector<u_char> &&data);

extern void state_visuals(ENetPeer &peer, state &&state);

extern void tile_apply_damage(ENetEvent &event, state state, block &block, u_int value);

/*
* @brief set slot::count to nagative value if you want to remove an amount. 
* @return the remaining amount if exeeds 200. e.g. emplace(slot{0, 201}) returns 1.
*/
extern u_short modify_item_inventory(ENetEvent& event, ::slot slot);

extern void item_change_object(ENetEvent& event, ::state state);

extern void merge_object(ENetEvent& event, ::slot slot, const ::pos& pos, ::world &world);
extern void remove_object(ENetEvent& event, signed uid);
/* despawn a world drop without crediting it to the peer (unlike remove_object / pickup) */
extern void despawn_object(ENetEvent& event, signed uid);
extern int  add_object(ENetEvent& event, ::slot slot, const ::pos& pos, ::world &world);

extern void add_drop(ENetEvent &event, ::slot im, ::pos pos, ::world &world);

extern void send_tile_update(ENetEvent &event, state s, ::block &b, ::world& w);

/*
* @param speed actually just the particle color & visual, not the speed.
* @param id seems to be a 0xc8 multiplier for multiple particles. unsure.
*/
extern void send_particle_effect(ENetEvent &event, const ::pos& pos, ::pos speed, int id = 0xc8*0, float offset = 0.0f);

extern void remove_fire(ENetEvent &event, state state, ::block &block, ::world& world);

extern void fireworks(ENetEvent &event, const ::pos& pos);

void generate_world(::world &world, const std::string& name);

bool door_mover(::world &world, const ::pos &pos);

namespace blast
{
    void thermonuclear(::world &world, const std::string& name);
}
