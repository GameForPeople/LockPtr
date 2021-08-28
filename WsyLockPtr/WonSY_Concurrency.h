/*
	Copyright 2021, Won Seong-Yeon. All Rights Reserved.
		KoreaGameMaker@gmail.com
		github.com/GameForPeople
*/

#pragma once

#define WONSY_CONCURRENCY

#include <functional>
#include <shared_mutex>

#define NODISCARD [[nodiscard]]

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
		// LockPtr Ver 0.1 ������ Atomic Shared Ptr�� �׻� �����͸� �����ؼ� �������ִ� ������ �����ϱ� ���� �������̽� �õ�
		// LockPtr Ver 0.2 ���� �Ƿ�����, Lock���� CAS������ ó���ϱ� ��ƴٰ� �Ǵ�. shared_mutex �߰�, CAS ����, ���� �� FactoryFunc�� ó��
		// LockPtr Ver 0.3 shared_mutex�� unlock�� ó���ϴ� ��ü�� �Ҹ��ڷ� �����ϴ� Defet�� ����.
		// LockPtr Ver 1.0 unique_ptr�� �Ҹ��ڷ� unlock�� ó���ϵ��� �ϴ� ��ü�� ����
		// LockPtr Ver 1.1 Write�� �ʿ��� ���, data�� ���ڷ� �޴� Func�� �޾Ƽ� ó���ϰ�, �� �ɷ� �ش� �Լ� ���� 
		// LockPtr Ver 1.2 Key ���� ����, ����� ���� �ּ�ȭ�� �� �ֵ��� ��ü ���� ����

		using _TypePtr       = _Type*;
		using _TypeConstPtr  = _Type const*;
		using _TypeUniquePtr = std::unique_ptr< _Type >;
		using _TypeSharedPtr = std::shared_ptr< _Type >;
		using _ReadDataPtr   = std::unique_ptr< const _Type, std::function< void( const _TypeConstPtr ) > >;
		using _WriteDataPtr  = std::unique_ptr< _Type, std::function< void( _TypePtr ) > >;
#pragma endregion
#pragma region [ Public Func ]
		LockPtr( const std::function< _TypePtr() >& func )
			: m_data( nullptr )
			, m_lock()
		{
			std::lock_guard local( m_lock );
			
			if ( func )
			{
				m_data = func();
			}

			//if ( !m_data )
			//{
			//	WARN_LOG( "? ����" );
			//}

			if constexpr ( std::is_class< _Type >::value )
			{
				// WARN_LOG( "���� ���縦 �������� ���� ���, GetCopy �������̽� ��� ��, ���� ���� ������ ��� ���� ����ȭ�� ���� �����Ǿ���մϴ�. [ type : " + typeid( _Type ).name() + " ] " );
				
				//is_trivially_copyable... �� Ÿ�� ��ü��. ���� ���� ���� ���θ� Ȯ���Ҽ��� ����;;
				//static_assert( !std::is_class< _Type >::value, "Go Die!!" );
			}
		}

		~LockPtr()
		{
			std::lock_guard local( m_lock );
			if ( m_data ) { delete m_data; }
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
			// ���� ���簡 �������� ���� ���, ���⼭���� ������ �����̴�.
			std::shared_lock local( m_lock );
			return *m_data;
		}

		NODISCARD _TypePtr CopyPtr()
		{
			// ���� ���簡 �������� ���� ���, ���⼭���� ������ �����̴�.
			std::shared_lock local( m_lock );
			return new _Type( *m_data );
		}

		_TypeUniquePtr CopyUniquePtr()
		{
			// ���� ���簡 �������� ���� ���, ���⼭���� ������ �����̴�.
			std::shared_lock local( m_lock );
			return std::make_unique< _Type >( *m_data );
		}

		_TypeSharedPtr CopySharedPtr()
		{
			// ���� ���簡 �������� ���� ���, ���⼭���� ������ �����̴�.
			std::shared_lock local( m_lock );
			return std::make_shared< _Type >( *m_data );
		}

		void Set( const _Type& data )
		{
			_TypePtr tempPtr;
			{
				std::lock_guard local( m_lock );
				tempPtr = m_data;
				m_data  = new _Type( data );
			}

			if ( tempPtr )
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