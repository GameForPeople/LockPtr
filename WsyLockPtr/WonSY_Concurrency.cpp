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
				std::cout << "[" << name << "] 주소 " << &cont << ", 요소 개수 : " << cont.size() << std::endl;
			};

		// 기본적인 사용법 및 개인적인 생각
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

			// _WriteDataPtr를 던지거나 Lock을 잡고 Task를 실행시키지 않고, Key만 제공하도록 인터페이스를 구성한 것은, 
			// 임계 영역과 관련한 자율성을 더 제공하는 유연한 코드의 작성이 가능할 것이라 생각하였습니다.
			{
				lockPtr.HelloWrite(
					[ &PrintFunc ]( auto& lockPtr, const WsyWriteKey& key )
					{
						// 아래 두 캐이스 중, 경우에 따라( 보통은 해야하는 작업의 비용에 따라 ) 선택하는 경우가 다르고, Key만 제공할 경우 유연한 코드 작성이 가능할 것으로 보였습니다.

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

			// 데이터의 복사 비용이 크지않고, 수정이 1개의 Context에서만 이루어지는 것이 보장된다면, 굳이 Hello-Get보다는, Copy-Set을 사용하는 것이, 더 올바른 판단일 것으로 보입니다.
			{
			}

			// Copy Funcs
			{
				auto copyValue = lockPtr.CopyValue();
				
				// 1.3 때 불필요한 Copy함수 모두 제거
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
										// 아래의 writePtr 아마 먼저 소유권을 가져가서, 여기서 바로 소유권을 얻지 못하고 3초간 대기한다.
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

			// 주의할 점
			if ( false )
			{
				//  LockPtr Ver 1.2에서 개선됨
				//// 0. GetForWrite와 Set은 DeadLock을 발생시킬 수 있다.
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

				// 1. 기존 atomic_shared_ptr과 동일하게 깊은 복사가 이루어지지 않는 영역에 대한 접근에서 ms는 성립하지 않습니다.
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
							// 보통은 임의의 다른 스레드문맥에서, 혹은 같더라도 내부 콜스택에서 임의로 변경할 경우.
							nodePtr.HelloWrite(
								[]( auto& nodePtr, const WsyWriteKey& key )
								{
									auto writePtr = nodePtr.Get( key );
									delete ( *writePtr ).m_ptr;
									( *writePtr ).m_ptr = nullptr;
								} );

							// 미정의 행동
							std::cout << *copyNode.m_ptr << std::endl;
						}
					}
				}

				//  LockPtr Ver 1.2에서 개선됨
				//// 2. Get 객체에 대하여, 소유권을 뺴돌릴 경우, 문제가 발생할 수 있습니다.
				//{
				//	const Cont* ref = [ &lockPtr, &PrintFunc ]()
				//		{
				//			auto tempPtr = lockPtr.GetForRead();
				//			PrintFunc( "C - 0", *tempPtr );
				//			return tempPtr.get();
				//		}();
				//
				//	// ???????? 으악!
				//	PrintFunc( "C - 1", *ref );
				//}

				// 3. Write 혹은 Read Task 도중에 같은 쓰레드에서 Set 혹은 Copy 할 경우, 데드락... 다만 이렇게 쓰면 애초에 이상하긴 한데;;
				{
					lockPtr.HelloRead(
						[]( auto& lockPtr, const WsyReadKey& key )
						{
							auto tempPtr  = lockPtr.Get( key );
							Cont getValue = *tempPtr.get();

							// 데드락
							lockPtr.Set( getValue );
						} );

					lockPtr.HelloWrite(
						[]( auto& lockPtr, const WsyWriteKey& key )
						{
							auto tempPtr = lockPtr.Get( key );

							// 데드락
							lockPtr.CopyValue();
						} );
				}
			}
		}
	}
}