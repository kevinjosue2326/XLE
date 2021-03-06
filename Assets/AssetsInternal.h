// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetSetInternal.h"
#include "AssetsCore.h"
#include "../Utility/IteratorUtils.h"
#include "../Utility/MemoryUtils.h"
#include "../Core/Types.h"
#include <vector>
#include <utility>
#include <string>
#include <sstream>

#if defined(ASSETS_STORE_DIVERGENT)
	#include "DivergentAsset.h"
#endif

namespace Assets { namespace Internal
{

///////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename... Params>
	std::basic_string<ResChar> AsString(Params... initialisers);

	template<typename Asset, typename... Params>
	std::basic_string<ResChar> BuildDescriptiveName(Params... initialisers)
	{
		return Internal::AsString(initialisers...);
	}

	template<typename Asset>
	std::basic_string<ResChar> BuildTargetFilename()
	{
		return std::basic_string<ResChar>();
	}

	template<typename Asset, typename... Params>
	std::basic_string<ResChar> BuildTargetFilename(Params... initialisers)
	{
		return Internal::AsString(std::get<0>(std::tuple<Params...>(initialisers...)));
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	std::shared_ptr<ICompileMarker> PrepareAsset(uint64 typeCode, const ResChar* initializers[], unsigned initializerCount);
    template<typename AssetType> using Ptr = std::unique_ptr<AssetType>;

    template <int DoCheckDependancy> struct CheckDependancy { template<typename Resource> static bool NeedsRefresh(const Resource* resource); };
    template<> struct CheckDependancy<1>   { template <typename Resource> static bool NeedsRefresh(const Resource* resource)   { return !resource || (resource->GetDependencyValidation()->GetValidationIndex()!=0); } };
    template<> struct CheckDependancy<0>   { template <typename Resource> static bool NeedsRefresh(const Resource* resource)   { return !resource; } };

    template <int BoBackgroundCompile> struct ConstructAsset {};

    template<> struct ConstructAsset<0>
    { 
        template<typename AssetType, typename... Params> 
			static typename Ptr<AssetType> Create(AssetSet<AssetType>&, uint64 hash, Params... initialisers)
		{
			return std::make_unique<AssetType>(std::forward<Params>(initialisers)...);
		}
    };

    template<> struct ConstructAsset<1>
    { 
        template<
            typename AssetType, typename... Params, 
            typename std::enable_if<!AssetTraits<AssetType>::HasIntermediateConstructor>::type* = nullptr>
            static typename Ptr<AssetType> Create(AssetSet<AssetType>&, uint64 hash, Params... initialisers)
        {
                // This asset type handles the compilation process manually. We will get a ICompileMarker
                // from the compiler and pass it directly to the asset.
            const char* inits[] = { ((const char*)initialisers)... };
            auto marker = PrepareAsset(GetCompileProcessType<AssetType>(), inits, dimof(inits));
            return std::make_unique<AssetType>(std::move(marker));
        }

        template<
            typename AssetType, typename... Params, 
            typename std::enable_if<AssetTraits<AssetType>::HasIntermediateConstructor>::type* = nullptr>
            static typename Ptr<AssetType> Create(AssetSet<AssetType>& set, uint64 hash, Params... initialisers)
        {
                // This asset type uses the default compilation process. The asset type itself only has
                // the logic for loading the completed intermediate asset. We will use general code for 
                // testing for existing assets and invoking compiles (etc).

            const char* inits[] = { ((const char*)initialisers)... };

            auto i = LowerBound(set._activeCompiles, hash);
            if (i != set._activeCompiles.end() && i->first == hash) {
                auto state = i->second._compileMarker->GetAssetState();
                if (state == AssetState::Pending)
                    Throw(Exceptions::PendingAsset(i->second._initializer.c_str(), "Compile still pending"));
                if (state == AssetState::Invalid)
                    Throw(Exceptions::PendingAsset(i->second._initializer.c_str(), "Asset became invalid during compile"));

                // note --  If we get an exception here, every subsequent call will follow this same path
                //          and reach this same invalid state.
                auto result = std::make_unique<AssetType>(i->second._compileMarker->GetLocator(), "CompiledAsset");
                set._activeCompiles.erase(i);
                return std::move(result);
            }

			auto marker = PrepareAsset(GetCompileProcessType<AssetType>(), inits, dimof(inits));
            auto existingLoc = marker->GetExistingAsset();
            if (!existingLoc._dependencyValidation || existingLoc._dependencyValidation->GetValidationIndex()!=0) {
                    // no existing asset (or out-of-date) -- we must invoke a compile
                auto pendingCompile = marker->InvokeCompile();
                auto initializer = marker->Initializer().AsString();
                set._activeCompiles.insert(i, std::make_pair(hash, ActiveCompileOperation{std::move(pendingCompile), initializer}));
                Throw(Exceptions::PendingAsset(initializer.c_str(), "Pending recompile"));
            }

            TRY {
                auto result = std::make_unique<AssetType>(existingLoc, "CompiledAsset");
                return std::move(result);
            } 
                
            // We should catch only some exceptions and force a recompile... This should happen on
            // missing file, or if the file has a bad version number. We also need to catch InvalidAsset,
            // because some assets will throw this on failure.
            // Note that other exceptions could be a problem here.
            CATCH (const Exceptions::InvalidAsset&) 
            {
                // LogWarning << "Asset (" << existingLoc._sourceID0 << ") appears to be invalid. Attempting recompile.";
            } 
            CATCH(const ::Assets::Exceptions::FormatError& e) 
            {
                if (e.GetReason() != ::Assets::Exceptions::FormatError::Reason::UnsupportedVersion)
                    throw;

                // LogWarning << "Asset (" << existingLoc._sourceID0 << ") appears to be incorrect version. Attempting recompile.";
            }
            CATCH(const Utility::Exceptions::IOException& e)
            {
                if (e.GetReason() != Utility::Exceptions::IOException::Reason::FileNotFound)
                    throw;

                // LogWarning << "Asset (" << existingLoc._sourceID0 << ") is missing. Attempting compile.";
            }
            CATCH_END

            // on invalid (eg, missing or out-of-date), we can try to invoke a recompile
            auto pendingCompile = marker->InvokeCompile();
            auto initializer = marker->Initializer().AsString();
            set._activeCompiles.insert(i, std::make_pair(hash, ActiveCompileOperation{std::move(pendingCompile), initializer}));
            Throw(Exceptions::PendingAsset(initializer.c_str(), "Pending recompile"));
        }
    };

	template <typename... Params> uint64 BuildHash(Params... initialisers);
	template <typename... Params> std::basic_string<ResChar> AsString(Params... initialisers);
    std::basic_string<ResChar> AsString();

    #pragma managed(push, off)
    //  Can work for the moment because upstream assets aren't handled.
    // template<typename AssetType, typename std::enable_if<AssetTraits<AssetType>::HasGetAssetState>::type* = nullptr>
    //     inline bool ReadyForReplacement(const AssetType& asset) { return asset.GetAssetState() != AssetState::Pending; }
    inline bool ReadyForReplacement(...) { return true; /* can't query, must do immediate replacement */ }
    #pragma managed(pop)

	template<bool DoCheckDependancy, bool DoBackgroundCompile, typename AssetType, typename... Params>
		const AssetType& GetAsset(AssetSetPtr<AssetType>& assetSet, Params... initialisers)
        {
                //
                //  This is the main bit of functionality in this file. Here we define
                //  the core behaviour when querying for an asset:
                //      * build a hash from the string inputs
                //      * if the asset already exists:
                //          * sometimes we check the invalidation state, and return a rebuilt asset
                //          * otherwise return the existing asset
                //      * otherwise we build a new asset
                //
			auto hash = BuildHash(initialisers...);

			#if defined(ASSETS_STORE_DIVERGENT)
					// divergent assets will always shadow normal assets
					// we also don't do a dependency check for these assets
				auto di = LowerBound(assetSet->_divergentAssets, hash);
				if (di != assetSet->_divergentAssets.end() && di->first == hash && di->second->HasChanges()) {
					return di->second->GetAsset();
				}
			#endif

            auto& assets = assetSet->_assets;
			auto i = LowerBound(assets, hash);
			if (i != assets.end() && i->first == hash) {
                auto& cnt = i->second;

                auto* checkForRefresh = cnt._active.get();
                if (cnt._pendingReplacement) checkForRefresh = cnt._pendingReplacement.get();
                if (CheckDependancy<DoCheckDependancy>::NeedsRefresh(checkForRefresh)) {
                        // note --  old resource will stay in memory until the new one has been constructed
                        //          If we get an exception during construct, we'll be left with a null ptr
                        //          in this asset set
                        //
                        //  There is a problem here whereby we retain the lock on the asset set while we are
                        //  initializing this create/compile operation. If it expensive, we could keep it
                        //  locked for awhile. Or, even worse, we could try for a recursive lock on the same
                        //  asset set.
                    cnt._pendingReplacement.reset();
                    cnt._pendingReplacement = ConstructAsset<DoBackgroundCompile>::Create<AssetType>(*assetSet.get(), hash, std::forward<Params>(initialisers)...);
                }

                // note that this will sometimes replace a "valid" asset with an "invalid" one
                if (!cnt._active || (cnt._pendingReplacement && ReadyForReplacement(*cnt._pendingReplacement)))
                    cnt._active = std::move(cnt._pendingReplacement);

                return *cnt._active;
            }

            #if defined(ASSETS_STORE_NAMES)
                auto name = AsString(initialisers...);  // (have to do this before constructor (incase constructor does std::move operations)
            #endif

            auto newAsset = ConstructAsset<DoBackgroundCompile>::Create<AssetType>(*assetSet.get(), hash, std::forward<Params>(initialisers)...);
            #if defined(ASSETS_STORE_NAMES)
                    // This is extra functionality designed for debugging and profiling
                    // attach a name to this hash value, so we can query the contents
                    // of an asset set and get meaningful values
                    //  (only insert after we've completed creation; because creation can throw an exception)
				InsertAssetName(assetSet->_assetNames, hash, name);
            #endif
                
                // we have to search again for the insertion point
                //  it's possible that while constructing the asset, we may have called GetAsset<>
                //  to create another asset. This can invalidate our iterator "i". If we had some way
                //  to test to make sure that the assetSet definitely hasn't changed, we coudl skip this.
                //  But just doing a size check wouldn't be 100% -- because there might be an add, then a remove
                //      (well, remove isn't possible currently. But it may happen at some point.
                //  For the future, we should consider threading problems, also. We will probably need
                //  a lock on the assetset -- and it may be best to release this lock while we're calling
                //  the constructor
            return *assetSet->Add(hash, std::move(newAsset));
        }

    template<bool DoCheckDependancy, bool DoBackgroundCompile, typename AssetType, typename... Params>
		const AssetType& GetAsset(Params... initialisers)
        {
            auto assetSet = GetAssetSet<AssetType>();
            return GetAsset<DoCheckDependancy, DoBackgroundCompile, AssetType, Params...>(assetSet, std::forward<Params>(initialisers)...);
        }

	template <typename AssetType, bool DoBackgroundCompile, typename... Params>
		std::shared_ptr<typename AssetTraits<AssetType>::DivAsset>& GetDivergentAsset(Params... initialisers)
		{
			#if !defined(ASSETS_STORE_DIVERGENT)
				throw ::Exceptions::BasicLabel("Could not get divergent asset, because ASSETS_STORE_DIVERGENT is not defined");
			#else

				auto hash = BuildHash(initialisers...);
				auto assetSet = GetAssetSet<AssetType>();
				auto di = LowerBound(assetSet->_divergentAssets, hash);
				if (di != assetSet->_divergentAssets.end() && di->first == hash) {
					return di->second;
				}

                typename AssetTraits<AssetType>::DivAsset::AssetIdentifier identifier;
                identifier._descriptiveName = BuildDescriptiveName<AssetType>(initialisers...);
                identifier._targetFilename = BuildTargetFilename<AssetType>(initialisers...);

                bool constructNewAsset = false;
                TRY {
					GetAsset<true, DoBackgroundCompile, AssetType>(assetSet, std::forward<Params>(initialisers)...);
                } CATCH (const Assets::Exceptions::InvalidAsset&) {
                    constructNewAsset = true;
                } CATCH_END

                if (constructNewAsset) {
					#if defined(ASSETS_STORE_NAMES)
						auto name = AsString(initialisers...);
					#endif

                        //  If we get an invalid asset, we have to create a new one
                        //  and assign it in place.
						//	note -- there's a problem here if the GetAsset<> above does
						//			a std::move() out of one of the parameters.
					assetSet->Add(hash, AssetType::CreateNew(std::forward<Params>(initialisers)...));

                    #if defined(ASSETS_STORE_NAMES)
					    InsertAssetNameNoCollision(assetSet->_assetNames, hash, name);
                    #endif
                }

				auto newDivAsset = std::make_shared<typename AssetTraits<AssetType>::DivAsset>(
					*assetSet, hash, identifier);

					// Do we have to search for an insertion point here again?
					// is it possible that constructing an asset could create a new divergent
					// asset of the same type? It seems unlikely
				assert(di == LowerBound(assetSet->_divergentAssets, hash));
				return assetSet->_divergentAssets.insert(di, std::make_pair(hash, std::move(newDivAsset)))->second;

			#endif
		}

    template <typename AssetType>
        AssetSet<AssetType>::AssetSet() 
        : _lock(CreateRecursiveMutexPtr())
    {}

    template <typename AssetType>
        AssetSet<AssetType>::~AssetSet() {}

    template <typename AssetType>
        void AssetSet<AssetType>::Clear() 
        {
            _assets.clear();
			#if defined(ASSETS_STORE_DIVERGENT)
				_divergentAssets.clear();
			#endif
            #if defined(ASSETS_STORE_NAMES)
                _assetNames.clear();
            #endif
        }

    template <typename AssetType>
        void AssetSet<AssetType>::LogReport() const 
        {
            LogHeader(unsigned(_assets.size()), typeid(AssetType).name());
            #if defined(ASSETS_STORE_NAMES)
                auto i = _assets.cbegin();
                auto ni = _assetNames.cbegin();
                unsigned index = 0;
                for (;i != _assets.cend(); ++i, ++index) {
                    while (ni->first < i->first && ni != _assetNames.cend()) { ++ni; }
                    if (ni != _assetNames.cend() && ni->first == i->first) {
                        LogAssetName(index, ni->second.c_str());
                    } else {
                        char buffer[256];
                        _snprintf_s(buffer, _TRUNCATE, "Unnamed asset with hash (0x%08x%08x)", 
                            uint32(i->first>>32), uint32(i->first));
                        LogAssetName(index, buffer);
                    }
                }
            #else
                auto i = _assets.cbegin();
                unsigned index = 0;
                for (;i != _assets.cend(); ++i, ++index) {
                    char buffer[256];
                    _snprintf_s(buffer, _TRUNCATE, "Unnamed asset with hash (0x%08x%08x)", 
                        uint32(i->first>>32), uint32(i->first));
                    LogAssetName(index, buffer);
                }
            #endif
        }

    template <typename AssetType>
        uint64          AssetSet<AssetType>::GetTypeCode() const
        {
            return typeid(AssetType).hash_code();
        }

    template <typename AssetType>
        const char*     AssetSet<AssetType>::GetTypeName() const
        {
            return typeid(AssetType).name();
        }

    template <typename AssetType>
        unsigned        AssetSet<AssetType>::GetDivergentCount() const
        {
            return unsigned(_divergentAssets.size());
        }

    template <typename AssetType>
        uint64          AssetSet<AssetType>::GetDivergentId(unsigned index) const
        {
            if (index < _divergentAssets.size()) return _divergentAssets[index].first;
            return ~0x0ull;
        }

    template <typename AssetType>
        bool            AssetSet<AssetType>::DivergentHasChanges(unsigned index) const
        {
            if (index < _divergentAssets.size()) return _divergentAssets[index].second->HasChanges();
            return false;
        }

    template <typename AssetType>
        std::string     AssetSet<AssetType>::GetAssetName(uint64 id) const
        {
            #if defined(ASSETS_STORE_NAMES)
                auto i = LowerBound(_assetNames, id);
                if (i != _assetNames.end() && i->first == id)
                    return i->second;
                return std::string();
            #else
                static std::string s_errorName("<<asset names disabled>>");
                return s_errorName;
            #endif
        }
}}

