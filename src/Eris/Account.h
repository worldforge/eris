#ifndef ERIS_PLAYER_H
#define ERIS_PLAYER_H

#include "Types.h"

#include "TransferInfo.h"

#include <Atlas/Objects/ObjectsFwd.h>

#include <sigc++/signal.h>

#include <vector>
#include <map>
#include <memory>

namespace Eris {

    class Connection;

    class Avatar;

    class AccountRouter;

    class TimedEvent;

    struct SpawnPoint;

/** Type used to return available characters */
    typedef std::map<std::string, Atlas::Objects::Entity::RootEntity> CharacterMap;

    typedef std::map<std::string, std::unique_ptr<Avatar>> ActiveCharacterMap;

/// Encapsulates all the state of an Atlas Account, and methods that operation on that state

/** An Account object represents the encapsulation of a server account, and it's binding to a character in the
game world. Future versions of Eris will support multiple Account objects per Connection, allowing various
configurations of interface, proxies and so forth.
<br>
Account is also the mechanism by which Lobby and Avatars objects are made available to the client,
in response to login / create operations */

class Account : virtual public sigc::trackable {
    public:
        /// Create a new Account associated with a Connection object
        /**
        Create a new Account object : currently only one is assumed, but multiple
        Accounts might be supported in the future
        @param con A valid (but not necessarily connected) Connection instance
        */
        explicit Account(Connection &con);

        virtual ~Account();

        /// Login to the server using user-supplied account information
        /** This is the basic way of logging into an existing account. Server-side
        failures during the login process, such as the account being unknown
        or an incorrect password being supplied, will result in the 'LoginFailure' signal being
        emitted with some vaugely helpful error message, and an error code. The LoginSuccess
        signal will be emitted upon sucessful completion of the login process.

        @param uname The username of the account
        @param pwd The correct password for the account
        */
        Result login(const std::string &uname, const std::string &pwd);

        /// Attempt to create a new account on the server and log into it.
        /* Create a new account on the server, if possible.
        Server-side failures, such as an account already existing with the specified
        username, will cause the 'LoginFailure' signal to be emitted with an error message
        and a code. As for 'login', LoginSuccess wil be emitted if everything goes as plan.

        @param uname The desired username of the account (eg 'ajr')
        @param fullName The real name of the user (e.g 'Al Riddoch')
        @param pwd The plaintext password for the new account
        */

        Result createAccount(const std::string &uname,
                             const std::string &fullName,
                             const std::string &pwd);

        /* Create a new account on the server, if possible.
        Server-side failures, such as an account already existing with the specified
        username, will cause the 'LoginFailure' signal to be emitted with an error message
        and a code. As for 'login', LoginSuccess wil be emitted if everything goes as plan.

        This variant allows you to specify your own Account op, which is useful when you
        want to create an account different from the standard one.

        @param accountOp The account operation, which will be wrapped in a "Create" op.
        */
        Result createAccount(const Atlas::Objects::Entity::Account &accountOp);


        /// Request logout from the server.
        /** Initiate a clean disconnection from the server. The LogoutComplete
        signal will be emitted when the process completes. Calling this on an Account
        which is not logged in will produce an error. */
        Result logout();

        /// Check if the account is logged in.
        /** Many operations will produce errors if the account is not logged in. */
        bool isLoggedIn() const;

        /// Get the characters owned by this account.
        /**
        Note you should call
        refreshCharacterInfo, and wait for the GotAllCharacters signal, prior to the
        initial call : otherwise, it will return an empty or incomplete list.
        */
        const CharacterMap &getCharacters();

        /**
        Update the character list (based on changes to play). The intention here is
        that clients will call this method for some kind of'choose character' interface
        or menu, and wait for the GotAllCharacters signal before displaying the list.
        Alternatively, you can display the UI immediately, and add character entries
        based on the GotCharacterInfo signal, which will be emitted once for each character.
        */
        Result refreshCharacterInfo();

        /// Transfer all characters to this account and then do all steps in takeCharacter()
        /**
        @param id The id of the game entity to transfer and activate
        @param key The possess_key to authenticate the game entity as ours
        */
        Result takeTransferredCharacter(const std::string &id, const std::string &key);

        /// Enter the game using an existing character
        /**
        @param id The id of the game entity to activate; this must be owned by the account.
        @result The Avatar that represents the character. Note ownership of this passes to
            the caller.
        */
        Result takeCharacter(const std::string &id);

        /// enter the game using a new character
        Result createCharacterThroughEntity(const Atlas::Objects::Entity::RootEntity &character);

        Result createCharacterThroughOperation(const Atlas::Objects::Operation::Create &c);

        /**
         * @brief Gets a list of active characters, i.e. entities on the server which the account can control.
         * @returns A list of active characters on the server which the account can control.
         */
        const ActiveCharacterMap &getActiveCharacters() const;

        /**
         * @brief Gets the available spawn points from where the client can create new characters.
         * @returns A store of available spawn points.
         */
        const std::vector<SpawnPoint> &getSpawnPoints() const;

        /// returns the account ID if logged in
        const std::string &getId() const;

        /** Return the username of this account. */
        const std::string &getUsername() const;

        /**
         * @brief Gets the parent type of the account.
         */
        const std::string &getParent() const;

        /// Access the underlying Connection for this account
        Connection &getConnection() const;

        /**
         * @brief Called when a logout of the avatar has been requested by the
         *  server.
         *
         * @note The avatar instance will be deleted by this method.
         *
         * @param avatar The avatar which is being logged out. This instance will
         *  be destroyed once this method is done.
         */
        void avatarLogoutRequested(Avatar *avatar);


// signals
        /// emitted when a character has been retrieved from the server
        sigc::signal<void(const Atlas::Objects::Entity::RootEntity &)> GotCharacterInfo;

        /// emitted when the entire character list had been updated
        sigc::signal<void()> GotAllCharacters;

        ///  Emitted when a server-side error occurs during account creation / login.
        /**
        The argument is an error message from the server - hopefully this will
        become something more useful such as an enum code, in the future.
        */
        sigc::signal<void(const std::string &)> LoginFailure;

        /** Emitted when login or character creation is successful. */
        sigc::signal<void()> LoginSuccess;

        /// Emitted when a logout completes
        /** Depending on whether the logout completed with a positive server
        acknowledgment or just timed out, the argument will be either true
        (success, clean logout) or false (failure, timeout or other problem)
        */
        sigc::signal<void(bool)> LogoutComplete;

        /**
        Emitted when creating a character or taking an existing one
        succeeds.
        */
        sigc::signal<void(Avatar *)> AvatarSuccess;

        /**
        Emitted when creating or taking a character fails for some reason.
        String argument is the error message from the server.
        */
        sigc::signal<void(const std::string &)> AvatarFailure;

        /**
        Emitted when an active avatar has been deactivated.
        */
        sigc::signal<void(const std::string &)> AvatarDeactivated;

        /**
         * Emitted when an error message is received.
         */
        sigc::signal<void(const std::string &)> ErrorMessage;

    protected:
        friend class AccountRouter;

        friend class Avatar; // so avata can call destroyCharacter

        void sightCharacter(const Atlas::Objects::Operation::RootOperation &op);

        void loginComplete(const Atlas::Objects::Entity::Account &p);

        void loginError(const Atlas::Objects::Operation::Error &err);

        Result internalLogin(const std::string &unm, const std::string &pwd);

        void internalLogout(bool clean);

        /// Callback for network re-establishment
        void netConnected();

        /// help! the plug is being pulled!
        bool netDisconnecting();

        void netFailure(const std::string &msg);

        void loginResponse(const Atlas::Objects::Operation::RootOperation &op);

        void logoutResponse(const Atlas::Objects::Operation::RootOperation &op);

        void possessResponse(const Atlas::Objects::Operation::RootOperation &op);

        void avatarCreateResponse(const Atlas::Objects::Operation::RootOperation &op);

        void avatarLogoutResponse(const Atlas::Objects::Operation::RootOperation &op);

        /**
         * @brief Destroys the avatar with the specified id, if available.
         *
         * A check is made if the avatar is registered with the account.
         *
         * Also emits AvatarDeactivated.
         * @param avatarId The avatar id.
         */
        void destroyAvatar(const std::string &avatarId);

        void handleLogoutTimeout();
//  void recvRemoteLogout(const Atlas::Objects::Operation::Logout &lo);

        void handleLoginTimeout();

        enum class Status {
            DISCONNECTED = 0,   ///< Default state, no server account active
            LOGGING_IN,         ///< Login sent, waiting for initial INFO response
            LOGGED_IN,          ///< Fully logged into a server-side account
            LOGGING_OUT,        ///< Sent a logout op, waiting for the INFO response

            TAKING_CHAR,        ///< sent a LOOK op for a character, awaiting INFO response
            CREATING_CHAR,      ///< send a character CREATE op, awaiting INFO response
            CREATED_CHAR        ///< a character was created, we now need to possess it
        };

        void internalDeactivateCharacter(const std::string& avatarId);

        virtual void updateFromObject(const Atlas::Objects::Entity::Account &p);

        Connection &m_con;  ///< underlying connection instance
        Status m_status;    ///< what the Player is currently doing
        std::unique_ptr<AccountRouter> m_router;

        std::string m_accountId;    ///< the account ID
        std::string m_username; ///< The player's username ( != account object's ID)
        std::string m_pass;

        std::string m_parent;
        CharacterMap _characters;   ///< characters belonging to this player
        std::set<std::string> m_characterIds;
        bool m_doingCharacterRefresh; ///< set if we're refreshing character data

        ActiveCharacterMap m_activeAvatars;
        std::unique_ptr<TimedEvent> m_timeout;

        /**
         * @brief A map of available spawn points.
         * These are points from which a new avatar can be created.
         */
        std::vector<SpawnPoint> m_spawnPoints;
    };

    inline const ActiveCharacterMap &Account::getActiveCharacters() const {
        return m_activeAvatars;
    }

    inline const std::string &Account::getId() const {
        return m_accountId;
    }

    inline const std::string &Account::getUsername() const {
        return m_username;
    }

    inline const std::string &Account::getParent() const {
        return m_parent;
    }


    inline Connection &Account::getConnection() const {
        return m_con;
    }

    inline const std::vector<SpawnPoint> &Account::getSpawnPoints() const {
        return m_spawnPoints;
    }


} // of namespace Eris

#endif
