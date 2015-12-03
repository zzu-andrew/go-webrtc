/**
 * C wrapper for the C++ PeerConnection code, to be go-compatible.
 */
#include "cpeerconnection.h"
// #include "webrtc/base/thread.h"
#include "webrtc/base/common.h"
#include "webrtc/base/common.h"
// #include "talk/app/webrtc/peerconnection.h"
// #include "talk/app/webrtc/peerconnectionfactory.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include <iostream>
#include <unistd.h>
#include <future>

#define SUCCESS 0
#define FAILURE 1

using namespace std;
using namespace webrtc;

// Smaller typedefs
typedef rtc::scoped_refptr<webrtc::PeerConnectionInterface> PC;
typedef SessionDescriptionInterface* SDP;

// Peer acts as the glue between go and native code PeerConnectionInterface.
// However, it is not directly accessible to cgo.
class Peer
  : public CreateSessionDescriptionObserver,
    public PeerConnectionObserver {

 public:

  void Initialize() {
    // OnSDP = async(launch::async, &Peer::OnSuccess, this);
    promiseSDP = promise<SDP>();
    // promiseSDP.set_value(NULL);
  }

  /*
   * Stub out all callbacks to become blocking, and return boolean success / fail.
   * Since the user wants to write go code, it'd be better to support goroutines
   * instead of callbacks.
   * This prevents the complication of casting Go function pointers and
   * then dealing with the risk of concurrently calling Go code from C from Go...
   * Which should be a much easier and safer for users of this library.
   * TODO(keroserene): Expand on this if there are more complicated callbacks.
   */
  Callback SuccessCallback = NULL;
  Callback FailureCallback = NULL;


  //
  // CreateSessionDescriptionObserver implementation
  //
  void OnSuccess(SDP desc) {
    cout << "SDP successfully created: " << desc << endl;
    promiseSDP.set_value(desc);
    // if (this->SuccessCallback) {
      // this->SuccessCallback();
    // }
  }
  void OnFailure(const std::string& error) {
    cout << "failure: " << error << endl;
    promiseSDP.set_value(NULL);
    // if (this->FailureCallback) {
      // this->FailureCallback();
    // }
  }

  //
  // PeerConnectionObserver Implementation
  //
  void OnStateChange(PeerConnectionObserver::StateType state) {
    cout << "OnStateChange" << endl;
  }

  void OnAddStream(webrtc::MediaStreamInterface* stream) {
    cout << "OnAddStream" << endl;
  }

  void OnRemoveStream(webrtc::MediaStreamInterface* stream) {
    cout << "OnRemoveStream" << endl;
  }

  void OnRenegotiationNeeded() {
    cout << "OnRenegotiationNeeded" << endl;
  }

  void OnIceCandidate(const IceCandidateInterface* candidate) {
    cout << "OnIceCandidate" << candidate << endl;
  }

  void OnDataChannel(DataChannelInterface* data_channel) {
    cout << "OnDataChannel" << endl;
  }

  PeerConnectionInterface::RTCConfiguration *config;
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  const MediaConstraintsInterface* constraints;

  PC pc_;  // This scoped_refptr must live in an object of some sort, or it will
           // be prematurely deallocated.

  // Passing SDPs through promises instead of callbacks.
  future<SDP> OnSDP;
  promise<SDP> promiseSDP;

  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory;
  // TODO: prepare and expose IceServers for real.
  PeerConnectionInterface::IceServers ice_servers;
};

// TODO: Wrap as much as possible within the Peer class?
rtc::scoped_refptr<Peer> peer;


// Create and return a PeerConnection object.
// This cannot be a method in |Peer|, because this must be accessible to cgo.
PeerConnection NewPeerConnection() {
  // peer = new Peer();
  peer = new rtc::RefCountedObject<Peer>();
  peer->Initialize();
  // TODO: Maybe need to use the more complex constructor with rtc::threads.
  peer->pc_factory = CreatePeerConnectionFactory();
  if (!peer->pc_factory.get()) {
    cout << "ERROR: Could not create PeerConnectionFactory" << endl;
    return NULL;
  }
  // PortAllocatorFactoryInterface *allocator;

  PeerConnectionInterface::IceServer *server = new
      PeerConnectionInterface::IceServer();
  server->uri = "stun:stun.l.google.com:19302";
  peer->ice_servers.push_back(*server);

  // Prepare RTC Configuration object. This is just the default one, for now.
  // TODO: A Go struct that can be passed and converted here.
  peer->config = new PeerConnectionInterface::RTCConfiguration();
  peer->config->servers = peer->ice_servers;
  // cout << "Preparing RTCConfiguration..." << peer->config << endl;

  // Prepare a native PeerConnection object.
  peer->pc_ = peer->pc_factory->CreatePeerConnection(
    *peer->config,
    peer->constraints,
    NULL,  // port allocator
    NULL,  // TODO: DTLS
    peer
    );
  if (!peer->pc_.get()) {
    cout << "ERROR: Could not create PeerConnection." << endl;
    return NULL;
  }
  cout << "[C] Made a PeerConnection: " << peer->pc_ << endl;
  return (PeerConnection)peer;
}


// Blocking version of CreateOffer (or, will be soon)
// Returns 0 on success, -1 on failure.
int CreateOffer(PeerConnection pc) {
  PC cPC = ((Peer*)pc)->pc_;
  cout << "[C] CreateOffer" << peer << endl;

  auto r = peer->promiseSDP.get_future();
  // TODO: Provide an actual RTCOfferOptions as an argument.
  cPC->CreateOffer(peer, peer->options);
  // sleep(5);
  // peer->promiseSDP.set_value(NULL);
  // async(cPC->CreateOffer(peer, peer->options);
  // async(&PeerConnectionInterface::CreateOffer, cPC, peer, peer->options);
  // TODO: Up in PeerConnectionFactory, should probably use custom threads in
  // order for the callbacks to be *actually* registered correctly.
  SDP sdp = r.get();  // blocking
  // SDP sdp = OnSDP.get();  // blocking
  cout << "[C] CreateOffer done! SDP: " << sdp << endl;
  return SUCCESS;
}

int CreateAnswer(PeerConnection pc) {
  PC cPC = ((Peer*)pc)->pc_;
  cout << "[C] CreateAnswer" << peer << endl;

  cPC->CreateAnswer(peer, peer->constraints);

  // TODO: Up in PeerConnectionFactory, should probably use custom threads in
  // order for the callbacks to be *actually* registered correctly.
  cout << "[C] CreateAnswer done!" << endl;
  return SUCCESS;
}
