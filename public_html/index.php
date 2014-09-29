<?php

/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


    define("CS_DCT_DB_DEFAULT_MAX_SHMEM_TIMEOUT",                      5);

    define("CS_OFF_DB_SHMEM_HEAD_STATUS",                              0);
    define("CS_OFF_DB_SHMEM_HEAD_SIGNAL",                              1);
    define("CS_OFF_DB_SHMEM_HEAD_CLIENT_COUNT",                        2);
    define("CS_OFF_DB_SHMEM_HEAD_MAX_CLIENT_COUNT",                    3);
    define("CS_OFF_DB_SHMEM_HEAD_REQUEST_PROCESS_ID",                  4);
    define("CS_OFF_DB_SHMEM_HEAD_RESPONSE_PROCESS_ID",                 5);
    define("CS_OFF_DB_SHMEM_HEAD_RESPONSE_SLOT_ID",                    6);
    define("CS_OFF_DB_SHMEM_HEAD_KEY_SIZE",                            8);
    define("CS_OFF_DB_SHMEM_HEAD_VALUE_SIZE",                          9);
    define("CS_OFF_DB_SHMEM_HEAD_ROW_SIZE",                           10);
    define("CS_OFF_DB_SHMEM_HEAD_SLOT_SIZE",                          11);
    define("CS_OFF_DB_SHMEM_HEAD_MAX_ROWS",                           12);
    define("CS_OFF_DB_SHMEM_HEAD_DATA_OFFSET",                        16);
    define("CS_OFF_DB_SHMEM_HEAD_SLOT_DATA_OFFSET",                   17);
    define("CS_OFF_DB_SHMEM_HEAD_ROW_DATA_OFFSET",                    18);
    define("CS_OFF_DB_SHMEM_HEAD_FIELD_COUNT",                        32);

    define("CS_OFF_DB_SHMEM_SLOT_PROCESS_ID",                          0);
    define("CS_OFF_DB_SHMEM_SLOT_ROW_COUNT",                           1);
    define("CS_OFF_DB_SHMEM_SLOT_RESULT",                              2);
    define("CS_OFF_DB_SHMEM_SLOT_TIMESTAMP",                           3);
    define("CS_OFF_DB_SHMEM_SLOT_DATA",                                8);

    define("CS_OFF_DB_SHMEM_ROW_REQUEST",                              0);
    define("CS_OFF_DB_SHMEM_ROW_RESPONSE",                             1);
    define("CS_OFF_DB_SHMEM_ROW_KEY_SIZE",                             2);
    define("CS_OFF_DB_SHMEM_ROW_VALUE_SIZE",                           3);
    define("CS_OFF_DB_SHMEM_ROW_DATA",                                 4);

    define("CS_PRT_DB_SHMEM_SLOT_RESULT_UNDEFINED",                       0);
    define("CS_PRT_DB_SHMEM_SLOT_RESULT_SUCCESS",                         1);
    define("CS_PRT_DB_SHMEM_SLOT_RESULT_ERROR",                          16);

    define("CS_PRT_DB_SHMEM_ROW_REQUEST_READ",                            1);
    define("CS_PRT_DB_SHMEM_ROW_REQUEST_WRITE",                           2);
    define("CS_PRT_DB_SHMEM_ROW_REQUEST_DELETE",                          3);
    define("CS_PRT_DB_SHMEM_ROW_REQUEST_COMMIT",                          4);
    define("CS_PRT_DB_SHMEM_ROW_REQUEST_CLOSE",                          16);

    define("CS_PRT_DB_SHMEM_ROW_RESPONSE_UNDEFINED",                      0);
    define("CS_PRT_DB_SHMEM_ROW_RESPONSE_SUCCESS",                        1);
    define("CS_PRT_DB_SHMEM_ROW_RESPONSE_NULL",                           1);
    define("CS_PRT_DB_SHMEM_ROW_RESPONSE_NOT_NULL",                       2);
    define("CS_PRT_DB_SHMEM_ROW_RESPONSE_ERROR",                         16);

    define("CS_DCT_CDB_GENESIS_TX_ASSET_MAP",                          0x01);
    define("CS_DCT_CDB_GENESIS_METADATA",                              0x02);
    define("CS_DCT_CDB_BLOCK_BY_HASH",                                 0x03);
    define("CS_DCT_CDB_BLOCK_BY_INDEX",                                0x04);
    define("CS_DCT_CDB_LAST_BLOCK",                                    0x80);
    define("CS_DCT_CDB_TRACKED_ASSET",                                 0x81);
    define("CS_DCT_CDB_ALL_ASSETS",                                    0x82);

    define("CS_ERR_PARSE_ERROR",                                     -32700);
    define("CS_ERR_INVALID_REQUEST",                                 -32600);
    define("CS_ERR_METHOD_NOT_FOUND",                                -32601);
    define("CS_ERR_INVALID_PARAMS",                                  -32602);
    define("CS_ERR_INTERNAL_ERROR",                                  -32603);
    
    define("CS_ERR_ASSET_NOT_FOUND",                                 -10000);
    define("CS_ERR_TXOUT_NOT_FOUND",                                 -10001);
    
    
    
    global $shmem_segment;


    function shmem_get_number($offset)
    {
        global $shmem_segment;
        $arr=unpack("V",shmop_read($shmem_segment,$offset*8,4));
        return $arr[1];
    }
    

    function shmem_set_number($offset,$value)
    {
        global $shmem_segment;
        shmop_write($shmem_segment,pack("V",$value),$offset*8);
    }
    

    function swap_bytes($str)
    {
        $len=strlen($str);
        $res="";
        for($i=0;$i<$len;$i++)
        {
            $res.=substr($str,$len-$i-1,1);
        }
        return $res;
    }
    

    function db_open()
    {
        global $shmem_segment;
        
        $slot=-1;
        if(isset($_GET['k']))
        {
            @$shmem_segment=shmop_open($_GET['k'],'w',0666,0); 
        }
        else
        {
            @$shmem_segment=shmop_open(0x3133,'w',0666,0); 
        }
        if(@$shmem_segment === false)
        {            
            return -1;
        }
        
        $start_time=time();
        $pid=getmypid();


        while(($slot<0) && (time()-$start_time<CS_DCT_DB_DEFAULT_MAX_SHMEM_TIMEOUT))
        {
            if(shmem_get_number(CS_OFF_DB_SHMEM_HEAD_RESPONSE_PROCESS_ID)!=$pid)
            {
                shmem_set_number(CS_OFF_DB_SHMEM_HEAD_REQUEST_PROCESS_ID,$pid);
            }
            else
            {
                $slot=shmem_get_number(CS_OFF_DB_SHMEM_HEAD_RESPONSE_SLOT_ID);
                shmem_set_number(CS_OFF_DB_SHMEM_HEAD_REQUEST_PROCESS_ID,0);
                shmem_set_number(CS_OFF_DB_SHMEM_HEAD_RESPONSE_PROCESS_ID,0);
            }                
            usleep(1000);
        }

        if($slot<0)
        {
            return $slot;
        }
        
        return (shmem_get_number(CS_OFF_DB_SHMEM_HEAD_DATA_OFFSET)+$slot*shmem_get_number(CS_OFF_DB_SHMEM_HEAD_SLOT_SIZE))/8;        
    }
    
    function db_close($slot_offset)
    {
        global $shmem_segment;
        
        if($slot_offset>=0)
        {
            $pid=getmypid();
            if(shmem_get_number($slot_offset+CS_OFF_DB_SHMEM_SLOT_PROCESS_ID) == $pid)
            {
                shmem_set_number($slot_offset+CS_OFF_DB_SHMEM_SLOT_ROW_COUNT,0);
                shmem_set_number($slot_offset+CS_OFF_DB_SHMEM_SLOT_RESULT,CS_PRT_DB_SHMEM_SLOT_RESULT_UNDEFINED);            
                $row_offset=$slot_offset+shmem_get_number(CS_OFF_DB_SHMEM_HEAD_SLOT_DATA_OFFSET)/8;
                shmem_set_number($row_offset+CS_OFF_DB_SHMEM_ROW_RESPONSE,CS_PRT_DB_SHMEM_ROW_RESPONSE_UNDEFINED);
                shmem_set_number($row_offset+CS_OFF_DB_SHMEM_ROW_REQUEST,CS_PRT_DB_SHMEM_ROW_REQUEST_CLOSE);
                shmem_set_number($slot_offset+CS_OFF_DB_SHMEM_SLOT_ROW_COUNT,1);

                $start_time=time();

                while((shmem_get_number($slot_offset+CS_OFF_DB_SHMEM_SLOT_PROCESS_ID) == $pid) && (time()-$start_time<CS_DCT_DB_DEFAULT_MAX_SHMEM_TIMEOUT))
                {
                    usleep(1000);                
                }
            }
        }    

        if($shmem_segment)
        {
            shmop_close($shmem_segment);
        }        
    }
    
    function db_request($slot_offset,$read_results)
    {
        global $shmem_segment;
        
        $row_size=shmem_get_number(CS_OFF_DB_SHMEM_HEAD_ROW_SIZE)/8;
        $key_size=shmem_get_number(CS_OFF_DB_SHMEM_HEAD_KEY_SIZE);
        $value_size=shmem_get_number(CS_OFF_DB_SHMEM_HEAD_VALUE_SIZE);
        $max_rows=shmem_get_number(CS_OFF_DB_SHMEM_HEAD_MAX_ROWS);
        $row=0;
        $count=0;
        $request_keys=array();

        shmem_set_number($slot_offset+CS_OFF_DB_SHMEM_SLOT_ROW_COUNT,0);
        shmem_set_number($slot_offset+CS_OFF_DB_SHMEM_SLOT_RESULT,CS_PRT_DB_SHMEM_SLOT_RESULT_UNDEFINED);            
        $row_offset=$slot_offset+shmem_get_number(CS_OFF_DB_SHMEM_HEAD_SLOT_DATA_OFFSET)/8;
        foreach($read_results as $key=>$row_def)
        {
            shmem_set_number($row_offset+CS_OFF_DB_SHMEM_ROW_RESPONSE,CS_PRT_DB_SHMEM_ROW_RESPONSE_UNDEFINED);
            shmem_set_number($row_offset+CS_OFF_DB_SHMEM_ROW_REQUEST,CS_PRT_DB_SHMEM_ROW_REQUEST_READ);
            shmem_set_number($row_offset+CS_OFF_DB_SHMEM_ROW_KEY_SIZE,$key_size);
            shmop_write($shmem_segment, $row_def['key'], ($row_offset+CS_OFF_DB_SHMEM_ROW_DATA)*8);
            $request_keys[$row]=$key;

            $row_offset+=$row_size;
            $row++;
            $count++;
            if(($row == $max_rows) || ($count == count($read_results)))
            {
                shmem_set_number($slot_offset+CS_OFF_DB_SHMEM_SLOT_ROW_COUNT,0);
                shmem_set_number($slot_offset+CS_OFF_DB_SHMEM_SLOT_RESULT,CS_PRT_DB_SHMEM_SLOT_RESULT_UNDEFINED);            
                shmem_set_number($slot_offset+CS_OFF_DB_SHMEM_SLOT_ROW_COUNT,$row);
                $start_time=time();
                while((shmem_get_number($slot_offset+CS_OFF_DB_SHMEM_SLOT_RESULT) == CS_PRT_DB_SHMEM_SLOT_RESULT_UNDEFINED) && (time()-$start_time<CS_DCT_DB_DEFAULT_MAX_SHMEM_TIMEOUT))
                {
                    usleep(1000);                
                }
                $row_offset=$slot_offset+shmem_get_number(CS_OFF_DB_SHMEM_HEAD_SLOT_DATA_OFFSET)/8;
                foreach($request_keys as $rkey)
                {
                    if(shmem_get_number($row_offset+CS_OFF_DB_SHMEM_ROW_RESPONSE)==CS_PRT_DB_SHMEM_ROW_RESPONSE_NOT_NULL)
                    {
                        $read_results[$rkey]['value']=shmop_read($shmem_segment, ($row_offset+CS_OFF_DB_SHMEM_ROW_DATA)*8+$key_size, $value_size);
                    }
                    $row_offset+=$row_size;                
                }
                $row_offset=$slot_offset+shmem_get_number(CS_OFF_DB_SHMEM_HEAD_SLOT_DATA_OFFSET)/8;
                $row=0;
                $request_keys=array();
            }
        }
        
        return $read_results;
    }
    
    function hex_to_bin($str)
    {
        $map=array(
            '0'=>0,
            '1'=>1,
            '2'=>2,
            '3'=>3,
            '4'=>4,
            '5'=>5,
            '6'=>6,
            '7'=>7,
            '8'=>8,
            '9'=>9,
            'a'=>10,
            'b'=>11,
            'c'=>12,
            'd'=>13,
            'e'=>14,
            'f'=>15,
        );
        $str=  strtolower($str);
        $len=strlen($str)/2;
        $res='';
        for($i=0;$i<$len;$i++)
        {
            $res.=chr($map[substr($str,2*$i+0,1)]*16+$map[substr($str,2*$i+1,1)]);
        }                
        return $res;
    }

    
    function array_to_object($arr)
    {
        $obj=new stdClass();
        foreach($arr as $key => $value)
        {
            if(strlen($key))
            {
                $obj->$key=$value;
            }
        }
        
        return $obj;
    }
    
    
    $decoded=null;
    
    switch($_SERVER['REQUEST_METHOD'])
    {
        case 'POST':
            $payload=file_get_contents('php://input');
            @$decoded=json_decode($payload);
            break;
        case 'GET':
            $decoded=new stdClass();
            $decoded->method='info';
            $decoded->id=null;
            if(isset($_GET['m']))
            {
                $decoded->method=$_GET['m'];
            }
            break;
    }
           
    
    $response=array();
    $response['error']=null;
    if(property_exists($decoded, "id"))
    {
        $response['id']=$decoded->id;                
    }

    if(is_null($decoded))
    {
        $response['error']=new stdClass();
        $response['error']->code=CS_ERR_PARSE_ERROR;
        $response['error']->message="Could not parse the JSON input received";
    }
    else
    {
        if(is_object($decoded))
        {
            if(property_exists($decoded, "method"))
            {
                $response['method']=$decoded->method;
            }
            else
            {
                $response['error']=new stdClass();
                $response['error']->code=CS_ERR_INVALID_REQUEST;
                $response['error']->message="The JSON received doesn't have method field";                          
            }            
            if($decoded->method == 'coinspark_assets_get_qty')
            {
                if(property_exists($decoded, "id"))
                {
                    $response['id']=$decoded->id;                
                }
                else
                {
                    $response['error']=new stdClass();
                    $response['error']->code=CS_ERR_INVALID_REQUEST;
                    $response['error']->message="The JSON received doesn't have id field";                          
                }
                if(property_exists($decoded, "params"))
                {
                    if(property_exists($decoded->params, "assets"))
                    {
                        if(!is_array($decoded->params->assets))
                        {
                            $response['error']=new stdClass();
                            $response['error']->code=CS_ERR_INVALID_PARAMS;
                            $response['error']->message="The assets parameters should be array";                                              
                        }
                    }  
                    else
                    {
                        $response['error']=new stdClass();
                        $response['error']->code=CS_ERR_INVALID_PARAMS;
                        $response['error']->message="The parameters in JSON received doesn't have assets field";                                              
                    }
                    if(property_exists($decoded->params, "txouts"))
                    {
                        if(!is_array($decoded->params->txouts))
                        {
                            $response['error']=new stdClass();
                            $response['error']->code=CS_ERR_INVALID_PARAMS;
                            $response['error']->message="The txouts parameters should be array";                                              
                        }
                    }  
                    else
                    {
                        $response['error']=new stdClass();
                        $response['error']->code=CS_ERR_INVALID_PARAMS;
                        $response['error']->message="The parameters in JSON received doesn't have txouts field";                                              
                    }
                }
                else
                {
                    $response['error']=new stdClass();
                    $response['error']->code=CS_ERR_INVALID_REQUEST;
                    $response['error']->message="The JSON received doesn't have params field";                          
                }            
            }
        }
        else
        {
            $response['error']=new stdClass();
            $response['error']->code=CS_ERR_INVALID_REQUEST;
            $response['error']->message="The JSON received is JSON object";          
        }
    }
    
    if(is_null($response['error']))
    {    
        switch($decoded->method)
        {
            case 'coinspark_assets_get_qty':
                break;
            case 'info':
		break;
            case 'getlastblock':
                break;
            case 'getblockmetadata':
                break;
            default:
                $response['error']=new stdClass();
                $response['error']->code=CS_ERR_METHOD_NOT_FOUND;
                $response['error']->message="The method in the request is not available";
                break;
        }
    }
    
    $slot_offset=-1;
    
    if(is_null($response['error']))
    {    
        $slot_offset=db_open();

        if($slot_offset<0)
        {
            $response['error']=new stdClass();
            $response['error']->code=CS_ERR_INTERNAL_ERROR;
            $response['error']->message="Could not connect to database";
        }
    }
    
    $asset_ids=array();
    $asset_txids=array();
    $asset_triple=array();
    $response['debug']=array();
    
    if(is_null($response['error']))
    {
        $read_results=array();
        
        switch($decoded->method)
        {
            case 'coinspark_assets_get_qty':
                $btc_asset=new stdClass();
                $btc_asset->txid='BTC';
                $btc_asset->block=0;
                $btc_asset->offset=81;
                $btc_asset->genhash="00000000";

                $txouts=$decoded->params->txouts;
                $assets=array_merge(array($btc_asset),$decoded->params->assets);
                
                $asset_read_results=array();

                foreach($assets as $key => $asset)
                {
                    if(is_object($asset))
                    {
                        $asset_ids[$key]=pack("V",$asset->block).pack("V",$asset->offset).hex_to_bin($asset->genhash);
                        $asset_txids[$key]=$asset->txid;
                        $asset_triple[$asset_txids[$key]]=array(
                            'genesis_block' => $asset->block,
                            'genesis_offset' => $asset->offset,
                            'genesis_prefix' => $asset->genhash,                    
                        );
                        $hash=hash('sha256',$asset_ids[$key],1);
                        $hash=hash('sha256',$hash,1);
                        $hash=swap_bytes($hash);
                        $asset_triple[$asset_txids[$key]]['genesis_hash']=reset(unpack('H*', $hash));
                        for($chunk=0;$chunk<5;$chunk++)
                        {
                            $asset_read_results[$key.'-'.$chunk]=array(
                                'key'       => swap_bytes($hash).pack("V",0).pack("V",0).pack("V",0).pack("V",$chunk),
                                'value'     => null,                        
                            );
                        }
                    }
                    else
                    {
                        $asset_ids[$key]=null;
                        $asset_txids[$key]=$asset;
                        $read_results[$key]=array(
                            'key'       => swap_bytes(hex_to_bin($asset)).pack("V",0).pack("V",0).pack("V",0).pack("V",0),
                            'value'     => null,
                        );               
                    }

                }

                if(count($asset_read_results))
                {
                    $asset_read_results=db_request($slot_offset,$asset_read_results);
                    foreach($assets as $key => $asset)
                    {
                        if(is_object($asset))
                        {
                            $asset_def='';
                            for($chunk=0;$chunk<5;$chunk++)
                            {
                                if(!is_null($asset_read_results[$key.'-'.$chunk]['value']))
                                {
                                    $asset_def.=$asset_read_results[$key.'-'.$chunk]['value'];
                                }
                            }                    
                            if(strlen($asset_def)>=8)
                            {
                                $arr=unpack('V',substr($asset_def,4,4));
                                $metasize=intval($arr[1]);
                                $asset_triple[$asset_txids[$key]]['genesis_txid']=reset(unpack('H*',swap_bytes(substr($asset_def,8+$metasize,32))));
                            }
                        }
                    }            
                }

                if(count($read_results))
                {
                    $read_results=db_request($slot_offset,$read_results);
                    foreach($read_results as $key=>$row_def)
                    {
                        if(!is_null($row_def['value']))
                        {
                            $asset_ids[$key]=substr($row_def['value'],4,12);

                            $asset_triple[$asset_txids[$key]]=array();
                            $arr=unpack('V', $asset_ids[$key]);		
                            $asset_triple[$asset_txids[$key]]['genesis_block']=intval($arr[1]);
                            $arr=unpack('V',substr($asset_ids[$key],4,4));
                            $asset_triple[$asset_txids[$key]]['genesis_offset']=intval($arr[1]);
                            $asset_triple[$asset_txids[$key]]['genesis_prefix']=substr($asset_txids[$key],0,8);
                            
//                            $asset_ids[$key]=substr($asset_ids[$key],0,8).hex_to_bin(substr($asset_txids[$key],0,8));
                        }
                    }
                }

                $read_results=array();

                foreach($asset_ids as $key=>$asset_id)
                {
                    if(!is_null($asset_id))
                    {
                        foreach($txouts as $txkey=>$txout)
                        {
                            if(is_object($txout))
                            {
                                if(property_exists($txout, "txid") && property_exists($txout, "vout"))
                                {
                                    $rkey=$txout->txid.'-'.$txout->vout.'-'.$asset_txids[$key];
/*                                    
                                    $response['debug'][$rkey]=array(
                                        'txid' => $txout->txid,
                                        'vout' => $txout->vout,
                                        'asset' => bin2hex($asset_id),
                                    );
 * 
 */
                                    $read_results[$rkey]=array(
                                        'key'       => swap_bytes(hex_to_bin($txout->txid)).pack("V",$txout->vout).$asset_id,
                                        'value'     => null,
                                    );
                                }
                            }
                        }
                    }
                }    

                $read_results=db_request($slot_offset,$read_results);

                $last_asset="";

                
                $result=array();
                
                foreach($read_results as $key=>$row_def)
                {
                    
                    $karr=explode('-',$key);
                    if($karr[2]!=$last_asset)
                    {
                        $last_asset=$karr[2];
                        $result[$last_asset]=array();                
                    }
                    $row_def['units']=null;
                    $row_def['block']=null;
                    $row_def['spent']=null;

                    $zero_units=false;
                    if(is_null($row_def['value']))
                    {
//                        $response['debug'][$key]['isnull']=1;
                        if($last_asset!=$btc_asset->txid)
                        {
                            $row_def['value']=$read_results[$karr[0].'-'.$karr[1].'-'.$btc_asset->txid]['value'];
                            $zero_units=true;
//                            $response['debug'][$key]['zero']=1;
                        }                
                    }
                    else
                    {
//                        $response['debug'][$key]['isnull']=0;                        
                    }

                    if(!is_null($row_def['value']))
                    {
                        $arr=unpack('V', $row_def['value']);

                        $row_def['units']=intval($arr[1]);
//                        $response['debug'][$key]['units1']=$row_def['units'];
                        $arr=unpack('V', substr($row_def['value'],4,4));
                        $row_def['units']+=0x100000000*intval($arr[1]);
//                        $response['debug'][$key]['units2']=$row_def['units'];
                        $arr=unpack('V', substr($row_def['value'],8,4));
                        $row_def['block']=intval($arr[1]);
                        $arr=unpack('V', substr($row_def['value'],12,4));
                        $row_def['spent']=intval($arr[1]);

                        if($zero_units)
                        {
                            $row_def['units']=0;
                        }
                    }
                    
                    if(!is_null($row_def['units']))
                    {
                        $result[$last_asset][]=array_to_object(array(
                            'txid'      => $karr[0],
                            'vout'      => $karr[1],
                            'qty'       => $row_def['units'],                
                            'block'     => $row_def['block'],                
                            'spent'     => $row_def['spent'],                
                        ));                        
                    }
                    else
                    {
                        $asset_error=new stdClass();
                        $asset_error->code=CS_ERR_TXOUT_NOT_FOUND;
                        $asset_error->message="The transaction output specified could not be found";
                        $result[$last_asset][]=array_to_object(array(
                            'txid'      => $karr[0],
                            'vout'      => $karr[1],
                            'error'     => $asset_error
                        ));                        
                        
                    }
                }

                foreach($asset_ids as $key=>$asset_id)
                {
                    if(is_null($asset_id))
                    {
                        $result[$asset_txids[$key]]=array();
                        $asset_error=new stdClass();
                        $asset_error->code=CS_ERR_ASSET_NOT_FOUND;
                        $asset_error->message="This server does not track the asset specified";
                        foreach($txouts as $txout)
                        {
                            if(property_exists($txout, "txid") && property_exists($txout, "vout"))
                            {
                                $result[$asset_txids[$key]][]=array_to_object(array(
                                    'txid'      => $txout->txid,
                                    'vout'      => $txout->vout,
                                    'error'     => $asset_error
                                ));                    
                            }
                        }
                    }
                }    



                $response['result']=array_to_object($result);
                $response['assets']=array_to_object($asset_triple);
                break;
            case 'info':
                $hash=hash('sha256',"lastblock",1);
                $hash=hash('sha256',$hash,1);
                $read_results[0]=array(
                    'key'       => $hash.pack("V",0).pack("V",0).pack("V",0).pack("V",0),
                    'value'     => null,
                );               
                
                $found=false;
                
                $read_results=db_request($slot_offset,$read_results);
                
                foreach($read_results as $key=>$row_def)
                {
                    if(!is_null($row_def['value']))
                    {                        
                        $values=array();
                        for($id=0;$id<4;$id++)
                        {
                            $arr=unpack('V',substr($row_def['value'],$id*4,4));
                            $values[$id]=intval($arr[1]);
                        }
                        
                        if($values[0] == CS_DCT_CDB_LAST_BLOCK)
                        {
                            $found=true;
                            $response['lastblock']=array(
                                'block'        => $values[1],
                                'created_ts'   => $values[2],
                                'created'      => gmdate("Y-m-d H:i:s",$values[2]),
                                'age'          => time()-$values[2],
                                'processed_ts' => $values[3],
                                'processed'    => gmdate("Y-m-d H:i:s",$values[3]),
                                'delay'        => $values[3]-$values[2],
                                'processed_before' => time()-$values[3],
                            );
                        }
                    }
                }
                if(!$found)
                {
	            $response['error']=new stdClass();
        	    $response['error']->code=CS_ERR_INTERNAL_ERROR;
            	    $response['error']->message="Last block record not found";
                }
                
                $hash=hash('sha256',"trackedasset",1);
                $hash=hash('sha256',$hash,1);
                $read_results[0]=array(
                    'key'       => $hash.pack("V",0).pack("V",0).pack("V",0).pack("V",0),
                    'value'     => null,
                );               
                
                $found=false;
                
                $read_results=db_request($slot_offset,$read_results);
                
                foreach($read_results as $key=>$row_def)
                {
                    if(!is_null($row_def['value']))
                    {                        
                        $values=array();
                        for($id=0;$id<4;$id++)
                        {
                            $arr=unpack('V',substr($row_def['value'],$id*4,4));
                            $values[$id]=intval($arr[1]);
                        }
                        
                        if($values[0] == CS_DCT_CDB_TRACKED_ASSET)
                        {
                            $found=true;
                            $response['trackedasset']=array(
                                'all_assets'   => false, 
                                'block'        => $values[1],
                                'offset'       => $values[2],
                                'prefix'       => $values[3],
                            );
                            $response['trackedasset']['prefix_hex']=sprintf("%04X", $response['trackedasset']['prefix']);
                            $response['trackedasset']['asset_ref']=$response['trackedasset']['block'].'-'.
                                                                   $response['trackedasset']['offset'].'-'.
                                                                   $response['trackedasset']['prefix'];
                        }
                        
                        if($values[0] == CS_DCT_CDB_ALL_ASSETS)
                        {
                            $found=true;
                            $response['trackedasset']=array(
                                'all_assets'   => true, 
                            );
                        }
                    }
                }
                if(!$found)
                {
                    $response['error']=new stdClass();
                    $response['error']->code=CS_ERR_INTERNAL_ERROR;
                    $response['error']->message="Tracked asset record not found";
                }
                
                break;
                
            case 'getlastblock':
                $hash=hash('sha256',"lastblock",1);
                $hash=hash('sha256',$hash,1);
                $read_results[0]=array(
                    'key'       => $hash.pack("V",0).pack("V",0).pack("V",0).pack("V",0),
                    'value'     => null,
                );               
                
                $found=false;
                
                $read_results=db_request($slot_offset,$read_results);
                
                foreach($read_results as $key=>$row_def)
                {
                    if(!is_null($row_def['value']))
                    {                        
                        $values=array();
                        for($id=0;$id<4;$id++)
                        {
                            $arr=unpack('V',substr($row_def['value'],$id*4,4));
                            $values[$id]=intval($arr[1]);
                        }
                        
                        if($values[0] == CS_DCT_CDB_LAST_BLOCK)
                        {
                            $found=true;
                            $response['lastblock']=array(
                                'block'        => $values[1],
                                'created_ts'   => $values[2],
                                'created'      => gmdate("Y-m-d H:i:s",$values[2]),
                                'age'          => time()-$values[2],
                                'processed_ts' => $values[3],
                                'processed'    => gmdate("Y-m-d H:i:s",$values[3]),
                                'delay'        => $values[3]-$values[2],
                                'processed_before' => time()-$values[3],
                            );
                        }
                    }
                }
                if(!$found)
                {
                    $response['error']='Last block record not found';                                                    
                }
                break;
            case 'getblockmetadata':
                $found=false;
                $hash=null;
                if(isset($_GET['b']))
                {
                    if(strlen($_GET['b'])==64)
                    {
                        $hash=hex_to_bin($_GET['b']);
                    }
                    else
                    {
                        $hash=pack("V",intval($_GET['b'])).str_repeat("\x00", 28);
                        $hash=hash('sha256',$hash,1);
                        $hash=hash('sha256',$hash,1);
                        
                        for($id=0;$id<3;$id++)
                        {
                            $read_results[$id]=array(
                                'key'       => ($hash).pack("V",0).pack("V",0).pack("V",0).pack("V",$id),
                                'value'     => null,
                            );               
                        }
                        $read_results=db_request($slot_offset,$read_results);
                        $hash='';
                        foreach($read_results as $key=>$row_def)
                        {
                            if(!is_null($row_def['value']))
                            {                        
                                $arr=unpack('V',substr($row_def['value'],0,4));
                                $code=intval($arr[1]);
                                if($code == CS_DCT_CDB_BLOCK_BY_INDEX)
                                {    
                                    $hash.=substr($row_def['value'],4,12);
                                }
                            }
                        }
                        
                        if(strlen($hash) !=36)
                        {
                            $hash=null;
                        }
                        else
                        {
                            $hash=  swap_bytes(substr($hash,0,32));
                        }
                    }
                }
                
                if(!is_null($hash))
                {
                    $read_results[0]=array(
                        'key'       => swap_bytes($hash).pack("V",0).pack("V",0).pack("V",0).pack("V",0),
                        'value'     => null,
                    );               
                }                
                
                $read_results=db_request($slot_offset,$read_results);
                
                foreach($read_results as $key=>$row_def)
                {
                    if(!is_null($row_def['value']))
                    {                        
                        $values=array();
                        for($id=0;$id<4;$id++)
                        {
                            $arr=unpack('V',substr($row_def['value'],$id*4,4));
                            $values[$id]=intval($arr[1]);
                        }
                        
                        if($values[0] == CS_DCT_CDB_BLOCK_BY_HASH)
                        {
                            $found=true;
                            $response['block']=array(
                                'hash'         => bin2hex($hash),
                                'id'           => $values[1],
                                'created_ts'   => $values[2],
                                'created'      => gmdate("Y-m-d H:i:s",$values[2]),
                                'txcount'      => $values[3],
                            );
                        }
                    }
                }
                if(!$found)
                {
                    $response['error']='Block record not found';                                                    
                }
                break;
        }
    }
    
    db_close($slot_offset);

    if(is_null($response['error']))
    {
        unset($response['error']);
    }
    if(count($response['debug'])==0)
    {
        unset($response['debug']);        
    }

    switch($decoded->method)
    {
        case 'info':
?>    
<HTML>
    <HEAD>
        <META HTTP-EQUIV="Content-Type" CONTENT="text/html; charset=UTF-8" />
        <TITLE> CoinSpark Tracking Server</TITLE>
        <META HTTP-EQUIV="refresh" CONTENT="15">
    </HEAD>
    <BODY>
        <?php
            if(isset($response['error']))
            {
        ?>
            Error: <?php echo($response['error']->message); ?>
        <?php                
            }
            else 
            {
        ?>
            This server tracks <?php if($response['trackedasset']['all_assets'])echo("all assets");else echo("asset with reference: ".$response['trackedasset']['asset_ref']);?><BR /><BR />
            Last block: <?php echo($response['lastblock']['block']);?><BR />
            Last block time: <?php echo(gmdate("Y-m-d H:i:s",$response['lastblock']['created_ts'])." GMT");?><BR />
        <?php
            }
        ?>
    </BODY>
</HTML>


<?php
            break;
        default:
            echo json_encode(array_to_object($response));
            break;
    }

?>

