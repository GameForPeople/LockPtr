/*
	Copyright 2021, Won Seong-Yeon. All Rights Reserved.
		KoreaGameMaker@gmail.com
		github.com/GameForPeople
*/

#include "WonSY_Concurrency.h"

#include <map>
#include <iostream>

namespace WonSY::Concurrency
{
	void TestLockPtr()
	{
		using namespace std::chrono_literals;
		using Cont = std::map< std::string, int >;

		const auto PrintFunc = []( const std::string& name, const auto& cont )
			{
				std::cout << "[" << name << "] �ּ� " << &cont << ", ��� ���� : " << cont.size() << std::endl;
			};

		// �⺻���� ���� �� �������� ����
		{
			WsyLockPtr< Cont > lockPtr( []() { return new Cont(); } );

			if ( !lockPtr )
			{

			}

			// GetForWrite
			{
				lockPtr.HelloWrite(
					[ &PrintFunc ]( auto& lockPtr, const WsyWriteKey& key )
					{
						auto writePtr = lockPtr.Get( key );
						writePtr.get()->insert( { "A0", 0 } );
						PrintFunc( "A - 0", *( writePtr.get() ) );

					} );
			}

			// GetForRead
			{
				lockPtr.HelloRead(
					[ &PrintFunc ]( auto& lockPtr, const WsyReadKey& key )
					{
						auto readPtr = lockPtr.Get( key );
						
						// build Error! Only Read!
						//readPtr.get()->insert( { 1, 1 } );
						
						PrintFunc( "A - 1", *( readPtr.get() ) );
					}
				);
			}

			// _WriteDataPtr�� �����ų� Lock�� ��� Task�� �����Ű�� �ʰ�, Key�� �����ϵ��� �������̽��� ������ ����, 
			// �Ӱ� ������ ������ �������� �� �����ϴ� ������ �ڵ��� �ۼ��� ������ ���̶� �����Ͽ����ϴ�.
			{
				lockPtr.HelloWrite(
					[ &PrintFunc ]( auto& lockPtr, const WsyWriteKey& key )
					{
						// �Ʒ� �� ĳ�̽� ��, ��쿡 ����( ������ �ؾ��ϴ� �۾��� ��뿡 ���� ) �����ϴ� ��찡 �ٸ���, Key�� ������ ��� ������ �ڵ� �ۼ��� ������ ������ �������ϴ�.

						// case 1
						{
							for ( int i = 0; i < 10; ++i )
							{
								auto writePtr = lockPtr.Get( key );
								writePtr.get(); // something...
							}
						}

						// case 2
						{
							auto writePtr = lockPtr.Get( key );
							
							for ( int i = 0; i < 10; ++i )
							{
								writePtr.get(); // something...
							}
						}
					} );
			}

			// �������� ���� ����� ũ���ʰ�, ������ 1���� Context������ �̷������ ���� ����ȴٸ�, ���� Hello-Get���ٴ�, Copy-Set�� ����ϴ� ����, �� �ùٸ� �Ǵ��� ������ ���Դϴ�.
			{
			}

			// Copy Funcs
			{
				auto copyValue = lockPtr.CopyValue();
				
				// 1.3 �� ���ʿ��� Copy�Լ� ��� ����
				//auto copyPtr       = lockPtr.CopyPtr();
				//auto copyUniquePtr = lockPtr.CopyUniquePtr();
				//auto copySharedPtr = lockPtr.CopySharedPtr();

				copyValue.insert( { "A2", 0 } );
				//copyPtr->insert( { "A3", 0 } );
				//copyUniquePtr->insert( { "A4", 0 } );
				//copySharedPtr->insert( { "A5", 0 } );

				PrintFunc( "A - 2", copyValue );
				//PrintFunc( "A - 3", *copyPtr );
				//PrintFunc( "A - 4", *( copyUniquePtr.get() ) );
				//PrintFunc( "A - 5", *( copySharedPtr.get() ) );

				lockPtr.HelloRead(
					[ &PrintFunc ]( auto& lockPtr, const WsyReadKey& key )
					{
						auto readPtr = lockPtr.Get( key );
						PrintFunc( "A - 6", *( readPtr.get() ) );
					} );

				//delete copyPtr; // omg!
			}

			// Write
			{
				// Get And Write
				{
					std::thread th;
					{
						th = static_cast< std::thread >( [ &lockPtr, &PrintFunc ]()
							{
								std::this_thread::sleep_for( 1s );

								lockPtr.HelloRead(
									[ &PrintFunc ]( auto& lockPtr, const WsyReadKey& key )
									{
										// �Ʒ��� writePtr �Ƹ� ���� �������� ��������, ���⼭ �ٷ� �������� ���� ���ϰ� 3�ʰ� ����Ѵ�.
										auto readPtr = lockPtr.Get( key );
										PrintFunc( "B - 1", *( readPtr.get() ) );
									} );
							} );

						lockPtr.HelloWrite(
							[ &PrintFunc ]( auto& lockPtr, const WsyWriteKey& key )
							{
								auto writePtr = lockPtr.Get( key );

								std::this_thread::sleep_for( 2s );

								writePtr.get()->insert( { "B0", 0 } );

								std::this_thread::sleep_for( 2s );
								
								PrintFunc( "B - 0", *( writePtr.get() ) );
							} );
					}
					th.join();
				}

				// Copy And Write
				{
					std::thread th;
					{
						th = static_cast< std::thread >( [ &lockPtr, &PrintFunc ]()
							{
								std::this_thread::sleep_for( 1s );

								lockPtr.HelloRead(
									[ &PrintFunc ]( auto& lockPtr, const WsyReadKey& key )
									{
										// not Waiting
										auto readPtr = lockPtr.Get( key );
										PrintFunc( "B - 3 - 1", *( readPtr.get() ) );
									} );

								std::this_thread::sleep_for( 2s );

								lockPtr.HelloRead(
									[ &PrintFunc ]( auto& lockPtr, const WsyReadKey& key )
									{
										auto readPtr = lockPtr.Get( key );
										PrintFunc( "B - 3 - 2", *( readPtr.get() ) );
									} );
							} );

						auto copyValue = lockPtr.CopyValue();
						
						std::this_thread::sleep_for( 2s );

						copyValue.insert( { "B2", 1 } );

						std::this_thread::sleep_for( 2s );
						
						lockPtr.Set( copyValue );
						PrintFunc( "B - 2", copyValue );
					}
					th.join();
				}
			}

			// ������ ��
			if ( false )
			{
				//  LockPtr Ver 1.2���� ������
				//// 0. GetForWrite�� Set�� DeadLock�� �߻���ų �� �ִ�.
				//{
				//	{
				//		auto writePtr = lockPtr.GetForWrite();
				//		
				//		// dead Lock
				//		 auto readPtr = lockPtr.GetForRead();
				//	}
				//
				//	{
				//		auto readPtr = lockPtr.GetForRead();
				//
				//		// dead Lock
				//		auto writePtr = lockPtr.GetForWrite();
				//	}
				//
				//	{
				//		auto copyValue = lockPtr.CopyValue();
				//		auto readPtr   = lockPtr.GetForRead(); // or writePtr
				//		
				//		// dead Lock
				//		lockPtr.Set( copyValue );
				//	}
				//}

				// 1. ���� atomic_shared_ptr�� �����ϰ� ���� ���簡 �̷������ �ʴ� ������ ���� ���ٿ��� ms�� �������� �ʽ��ϴ�.
				{
					struct Node
					{
						int* m_ptr = new int ( 0 );
					};

					WsyLockPtr< Node > nodePtr( []() { return new Node(); } );

					{
						auto copyNode = nodePtr.CopyValue();
						if ( copyNode.m_ptr )
						{
							// ������ ������ �ٸ� �����幮�ƿ���, Ȥ�� ������ ���� �ݽ��ÿ��� ���Ƿ� ������ ���.
							nodePtr.HelloWrite(
								[]( auto& nodePtr, const WsyWriteKey& key )
								{
									auto writePtr = nodePtr.Get( key );
									delete ( *writePtr ).m_ptr;
									( *writePtr ).m_ptr = nullptr;
								} );

							// ������ �ൿ
							std::cout << *copyNode.m_ptr << std::endl;
						}
					}
				}

				//  LockPtr Ver 1.2���� ������
				//// 2. Get ��ü�� ���Ͽ�, �������� ������ ���, ������ �߻��� �� �ֽ��ϴ�.
				//{
				//	const Cont* ref = [ &lockPtr, &PrintFunc ]()
				//		{
				//			auto tempPtr = lockPtr.GetForRead();
				//			PrintFunc( "C - 0", *tempPtr );
				//			return tempPtr.get();
				//		}();
				//
				//	// ???????? ����!
				//	PrintFunc( "C - 1", *ref );
				//}

				// 3. Write Ȥ�� Read Task ���߿� ���� �����忡�� Set Ȥ�� Copy �� ���, �����... �ٸ� �̷��� ���� ���ʿ� �̻��ϱ� �ѵ�;;
				{
					lockPtr.HelloRead(
						[]( auto& lockPtr, const WsyReadKey& key )
						{
							auto tempPtr  = lockPtr.Get( key );
							Cont getValue = *tempPtr.get();

							// �����
							lockPtr.Set( getValue );
						} );

					lockPtr.HelloWrite(
						[]( auto& lockPtr, const WsyWriteKey& key )
						{
							auto tempPtr = lockPtr.Get( key );

							// �����
							lockPtr.CopyValue();
						} );
				}
			}
		}
	}
}