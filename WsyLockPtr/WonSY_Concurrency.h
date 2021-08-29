/*
	Copyright 2021, Won Seong-Yeon. All Rights Reserved.
		KoreaGameMaker@gmail.com
		github.com/GameForPeople
*/

#pragma once

#define WONSY_CONCURRENCY

#include <functional>
#include <shared_mutex>

#define NODISCARD  [[nodiscard]]
#define LIKELY     [[likely]]
#define UNLIKELY   [[unlikely]]

namespace WonSY::Concurrency
{
#pragma region [ LockPtr ]
	#define DELETE_COPY_AND_MOVE( type )      \
	type( const type& ) = delete;             \
	type& operator=( const type& ) = delete;  \
	type( type&& ) = delete;                  \
	type& operator=( type&& ) = delete;       \

	struct ReadKey { template < class _Type > friend struct LockPtr; private: explicit ReadKey()  = default; DELETE_COPY_AND_MOVE( ReadKey )  };
	struct WriteKey{ template < class _Type > friend struct LockPtr; private: explicit WriteKey() = default; DELETE_COPY_AND_MOVE( WriteKey ) };

	template < class _Type >
	struct LockPtr
	{
#pragma region [ Def ]
		// LockPtr Ver 0.1 기존의 Atomic Shared Ptr가 항상 데이터를 복사해서 전달해주는 단점을 개선하기 위한 인터페이스 시도
		// LockPtr Ver 0.2 현재 실력으로, Lock없이 CAS만으로 처리하기 어렵다고 판단. shared_mutex 추가, CAS 제거, 생성 시 FactoryFunc로 처리
		// LockPtr Ver 0.3 shared_mutex의 unlock을 처리하는 객체를 소멸자로 포함하는 Defer의 전달.
		// LockPtr Ver 1.0 unique_ptr의 소멸자로 unlock을 처리하도록 로직 변경
		// LockPtr Ver 1.1 Write가 필요할 경우, data를 인자로 받는 Func을 받아서 처리하고, 락 걸고 해당 함수 실행 
		// LockPtr Ver 1.2 Key 개념 적용, 데드락 상태 최소화할 수 있도록 전체 구조 개선
		// LockPtr Ver 1.3 비효율적으로 Set하던 로직 수정, 불필요한 Copy 계열 함수들 모두 제거, Like-Unlike

		// 개선이 필요한 점
		// 0. Type이 Map이라 가정할 떄, Map의 구조는 변경하지 않고, Value에 접근하여 변경할 때는 동기화 대상이 Map이 아니고, 해당 내부 Value이기 때문에 굳이 비싼 HelloWrite를 사용할 필요가 없음... ReadLock을 사용해도 TS함.
		// 1. Get보다는 Ref가 더 정확한 표현으로 보이는데, 관용적으로는 Get이 맞으니.. 더 좋은 네이밍에 대해서 전체적으로 고려할 필요가 있음..

		using _TypePtr       = _Type*;
		using _TypeConstPtr  = _Type const*;
		using _ReadDataPtr   = std::unique_ptr< const _Type, std::function< void( const _TypeConstPtr ) > >;
		using _WriteDataPtr  = std::unique_ptr< _Type, std::function< void( _TypePtr ) > >;
#pragma endregion
#pragma region [ Public Func ]
		LockPtr( const std::function< _TypePtr() >& func )
			: m_data( nullptr )
			, m_lock()
		{
			std::lock_guard local( m_lock );
			
			if ( func ) LIKELY
			{
				m_data = func();
			}

			//if ( !m_data )
			//{
			//	WARN_LOG( "? 뭐여" );
			//}

			if constexpr ( std::is_class< _Type >::value )
			{
				// WARN_LOG( "깊은 복사를 지원하지 않을 경우, GetCopy 인터페이스 사용 시, 내부 얕은 복사의 대상에 대한 동기화는 따로 구현되어야합니다. [ type : " + typeid( _Type ).name() + " ] " );
				
				//is_trivially_copyable... 아 타입 자체가. 깊은 복사 지원 여부를 확인할수가 없네;;
				//static_assert( !std::is_class< _Type >::value, "Go Die!!" );
			}
		}

		~LockPtr()
		{
			std::lock_guard local( m_lock );
			if ( m_data ) LIKELY { delete m_data; }
		}

		void HelloRead( const std::function< void( LockPtr< _Type >& lockPtr, const ReadKey& ) >& func )
		{
			ReadKey key;
			func( *this, key );
		}

		void HelloWrite( const std::function< void( LockPtr< _Type >& lockPtr, const WriteKey& ) >& func )
		{
			WriteKey key;
			func( *this, key );
		}

		NODISCARD _ReadDataPtr Get( const ReadKey& /* key */ )
		{
			m_lock.lock_shared();

			return std::unique_ptr< const _Type, std::function< void( const _TypeConstPtr ) > >(
				m_data,
				[ this ]( const _TypeConstPtr ptr ) noexcept -> void
				{
					{
						// m_data = ptr;
					}
					m_lock.unlock_shared();
				} );
		}

		NODISCARD _WriteDataPtr Get( const WriteKey& /* key */ )
		{
			m_lock.lock();

			return std::unique_ptr< _Type, std::function< void( _TypePtr ) > >(
				m_data,
				[ this ]( _TypePtr ptr ) noexcept -> void
				{
					{
						// ptr = nullptr;
						// m_data = ptr;
					}
					m_lock.unlock();
				} );
		}

		_Type CopyValue()
		{
			// 깊은 복사가 지원되지 않을 경우, 여기서부터 지옥의 시작이다.
			std::shared_lock local( m_lock );
			return *m_data;
		}

		void Set( const _Type& data )
		{
			_TypePtr tempPtr = new _Type( data );
			
			{
				std::lock_guard local( m_lock );
				std::swap( m_data, tempPtr );
			}

			if ( tempPtr ) LIKELY
				delete tempPtr;
		}

#pragma endregion
#pragma region [ Member Var ]
	private:
		_TypePtr          m_data;
		std::shared_mutex m_lock;
#pragma endregion
	};

	void TestLockPtr();
#pragma endregion
}

template < class _Type >
using WsyLockPtr = WonSY::Concurrency::LockPtr< _Type >;

using WsyReadKey  = WonSY::Concurrency::ReadKey;
using WsyWriteKey = WonSY::Concurrency::WriteKey;