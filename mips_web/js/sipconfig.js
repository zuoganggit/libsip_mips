
function sendHttpRequest(url, options) {
    return fetch(url, options)
      .then(response => {
        if (!response.ok) {
          throw new Error('Request failed. Status: ' + response.status);
        }
        return response.text();
      })
      .catch(error => {
        console.error('Error:', error);
      });
}

var html_con = ' \
<div class="form-floating mb-3 mt-3"> \
      <input value="192.168.10.89:5060"   type="text" class="form-control" id="domain" placeholder="Enter domain" name="domain">\
      <label for="domain">sip服务器域名</label>\
    </div>\
    <div class="form-floating mt-3 mb-3">\
      <input value="1001" type="text" class="form-control" id="user" placeholder="Enter user" name="user">\
      <label for="user">用户名</label>\
    </div>\
    <div class="form-floating mt-3 mb-3">\
      <input value="123456" type="text" class="form-control" id="pwd" placeholder="Enter pwd" name="pwd">\
      <label for="pwd">密码</label>\
    </div>\
    <div class="form-floating mt-3 mb-3">\
      <input value="3600" type="text" class="form-control" id="expires" placeholder="Enter expires" name="expires">\
      <label for="expires">注册有效期/秒</label>\
    </div>\
    <div class="form-floating mt-3 mb-3">\
      <input value="30" type="text" class="form-control" id="period" placeholder="Enter period" name="period">\
      <label for="period">注册周期/秒</label>\
    </div>\
    <div class="form-floating mt-3 mb-3">\
      <input value="1001" type="text" class="form-control" id="period" placeholder="Enter period" name="period">\
      <label for="period">外呼账户</label>\
    </div>\
    <div class="button-container">\
      <button type="submit" class="btn btn-primary">保存</button>\
    </div>\
'

function getSipconfigByHttp(){
    const domain = window.location.hostname
    return sendHttpRequest('http://' + domain + "/api/sipconfig", {
        method: 'GET',
        headers: {
        'Content-Type': 'application/json',
        },
    })
}


function ShowSipConfig(){
    getSipconfigByHttp().then(response =>{
        let json_data = JSON.parse(response).data
        console.log(json_data)
    })
}


function GetSipConfig(){

}

